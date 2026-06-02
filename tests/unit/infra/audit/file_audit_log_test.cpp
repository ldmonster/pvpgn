// SPDX-License-Identifier: GPL-2.0-or-later

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>

#include "domain/moderation/ports.hpp"
#include "infra/audit/file_audit_log.hpp"

namespace ap = pvpgn::domain::moderation;
using pvpgn::infra::audit::FileAuditLog;

namespace {

ap::AuditEntry make_entry(ap::AuditAction a, std::uint32_t actor,
                          std::string subject, std::string details,
                          std::string source_ip = "") {
    ap::AuditEntry e{};
    e.action    = a;
    e.actor     = pvpgn::domain::AccountId{actor};
    e.subject   = std::move(subject);
    e.details   = std::move(details);
    e.source_ip = std::move(source_ip);
    // fixed timestamp 2026-05-20T12:34:56Z  (0 ms)
    std::tm tm_buf{};
    tm_buf.tm_year = 2026 - 1900;
    tm_buf.tm_mon  = 5 - 1;
    tm_buf.tm_mday = 20;
    tm_buf.tm_hour = 12;
    tm_buf.tm_min  = 34;
    tm_buf.tm_sec  = 56;
#if defined(_WIN32)
    std::time_t t = ::_mkgmtime(&tm_buf);
#else
    std::time_t t = ::timegm(&tm_buf);
#endif
    e.timestamp = std::chrono::system_clock::from_time_t(t);
    return e;
}

std::string tmp_path(const char* tag) {
    auto p = std::filesystem::temp_directory_path() /
             (std::string("pvpgn_audit_") + tag + ".log");
    std::error_code ec;
    std::filesystem::remove(p, ec);
    return p.string();
}

}  // namespace

TEST_CASE("format_line is NDJSON and newline-terminated",
          "[infra][audit][file]") {
    auto e = make_entry(ap::AuditAction::AccountBanned, 42, "alice", "bot");
    auto line = FileAuditLog::format_line(e);
    CHECK(line ==
          "{\"ts\":\"2026-05-20T12:34:56.000Z\","
          "\"action\":\"AccountBanned\","
          "\"actor\":42,"
          "\"subject\":\"alice\","
          "\"details\":\"bot\"}\n");
}

TEST_CASE("format_line includes source_ip when non-empty",
          "[infra][audit][file]") {
    auto e = make_entry(ap::AuditAction::AccountBanned, 42, "alice", "bot",
                        "192.168.1.1");
    auto line = FileAuditLog::format_line(e);
    CHECK(line ==
          "{\"ts\":\"2026-05-20T12:34:56.000Z\","
          "\"action\":\"AccountBanned\","
          "\"actor\":42,"
          "\"subject\":\"alice\","
          "\"details\":\"bot\","
          "\"source_ip\":\"192.168.1.1\"}\n");
}

TEST_CASE("format_line omits source_ip when empty",
          "[infra][audit][file]") {
    auto e = make_entry(ap::AuditAction::AccountCreated, 1, "user", "");
    auto line = FileAuditLog::format_line(e);
    // source_ip field must not appear
    CHECK(line.find("source_ip") == std::string::npos);
}

TEST_CASE("format_line JSON-escapes special characters in fields",
          "[infra][audit][file]") {
    auto e = make_entry(ap::AuditAction::TopicChanged, 1,
                        "ch\tat", "line1\nline2\\done");
    auto line = FileAuditLog::format_line(e);
    CHECK(line ==
          "{\"ts\":\"2026-05-20T12:34:56.000Z\","
          "\"action\":\"TopicChanged\","
          "\"actor\":1,"
          "\"subject\":\"ch\\tat\","
          "\"details\":\"line1\\nline2\\\\done\"}\n");
}

TEST_CASE("parse_line round-trips format_line for tricky strings",
          "[infra][audit][file]") {
    auto e = make_entry(ap::AuditAction::ChannelMemberKicked, 7,
                        "name\twith\ttabs", "msg\nwith\nnewlines",
                        "10.0.0.1");
    auto line = FileAuditLog::format_line(e);

    ap::AuditEntry parsed{};
    REQUIRE(FileAuditLog::parse_line(line, parsed));
    CHECK(parsed.action    == e.action);
    CHECK(parsed.actor     == e.actor);
    CHECK(parsed.subject   == e.subject);
    CHECK(parsed.details   == e.details);
    CHECK(parsed.source_ip == e.source_ip);
    CHECK(parsed.timestamp == e.timestamp);
}

TEST_CASE("parse_line round-trips entry with empty source_ip",
          "[infra][audit][file]") {
    auto e = make_entry(ap::AuditAction::ServerShutdown, 0, "", "ok");
    auto line = FileAuditLog::format_line(e);

    ap::AuditEntry parsed{};
    REQUIRE(FileAuditLog::parse_line(line, parsed));
    CHECK(parsed.action    == e.action);
    CHECK(parsed.subject   == e.subject);
    CHECK(parsed.details   == e.details);
    CHECK(parsed.source_ip == "");
    CHECK(parsed.timestamp == e.timestamp);
}

TEST_CASE("parse_line rejects malformed input", "[infra][audit][file]") {
    ap::AuditEntry out{};
    // Not JSON at all
    CHECK_FALSE(FileAuditLog::parse_line("only-two\tfields", out));
    // Missing required fields
    CHECK_FALSE(FileAuditLog::parse_line(
        "{\"ts\":\"2026-05-20T12:34:56.000Z\",\"action\":\"AccountBanned\","
        "\"actor\":1,\"subject\":\"s\"}",
        out));  // missing details
    // Bad timestamp
    CHECK_FALSE(FileAuditLog::parse_line(
        "{\"ts\":\"not-a-date\",\"action\":\"AccountBanned\","
        "\"actor\":1,\"subject\":\"s\",\"details\":\"d\"}",
        out));
    // Unknown action
    CHECK_FALSE(FileAuditLog::parse_line(
        "{\"ts\":\"2026-05-20T12:34:56.000Z\",\"action\":\"BogusAction\","
        "\"actor\":1,\"subject\":\"s\",\"details\":\"d\"}",
        out));
}

TEST_CASE("file audit log persists and replays entries",
          "[infra][audit][file]") {
    auto path = tmp_path("rw");

    {
        auto log = FileAuditLog::try_open(path);
        REQUIRE(log);
        log->record(make_entry(ap::AuditAction::AccountCreated, 1, "alice", ""));
        log->record(make_entry(ap::AuditAction::AccountBanned, 2, "bob", "spam",
                               "1.2.3.4"));
        log->record(make_entry(ap::AuditAction::ServerShutdown, 0, "", "ok"));
    }
    {
        auto log = FileAuditLog::try_open(path);
        REQUIRE(log);
        auto last2 = log->recent(2);
        REQUIRE(last2.size() == 2);
        CHECK(last2[0].action    == ap::AuditAction::AccountBanned);
        CHECK(last2[0].subject   == "bob");
        CHECK(last2[0].source_ip == "1.2.3.4");
        CHECK(last2[1].action    == ap::AuditAction::ServerShutdown);
        CHECK(last2[1].details   == "ok");

        auto all = log->recent(100);
        CHECK(all.size() == 3);
    }

    std::filesystem::remove(path);
}

TEST_CASE("file audit log recent(0) returns empty", "[infra][audit][file]") {
    auto path = tmp_path("empty");
    auto log  = FileAuditLog::try_open(path);
    REQUIRE(log);
    log->record(make_entry(ap::AuditAction::AccountCreated, 1, "a", ""));
    CHECK(log->recent(0).empty());
    std::filesystem::remove(path);
}
