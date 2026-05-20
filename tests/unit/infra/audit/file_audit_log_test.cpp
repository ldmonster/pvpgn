// SPDX-License-Identifier: GPL-2.0-or-later

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>

#include "application/ports/audit_log.hpp"
#include "infra/audit/file_audit_log.hpp"

namespace ap = pvpgn::application::ports;
using pvpgn::infra::audit::FileAuditLog;

namespace {

ap::AuditEntry make_entry(ap::AuditAction a, std::uint32_t actor,
                          std::string subject, std::string details) {
    ap::AuditEntry e{};
    e.action  = a;
    e.actor   = pvpgn::domain::AccountId{actor};
    e.subject = std::move(subject);
    e.details = std::move(details);
    // fixed timestamp 2026-05-20T12:34:56Z
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

TEST_CASE("format_line is tab-separated and newline-terminated",
          "[infra][audit][file]") {
    auto e = make_entry(ap::AuditAction::AccountBanned, 42, "alice", "bot");
    auto line = FileAuditLog::format_line(e);
    CHECK(line ==
          "2026-05-20T12:34:56Z\tAccountBanned\t42\talice\tbot\n");
}

TEST_CASE("format_line escapes tab/newline/backslash in fields",
          "[infra][audit][file]") {
    auto e = make_entry(ap::AuditAction::TopicChanged, 1,
                        "ch\tat", "line1\nline2\\done");
    auto line = FileAuditLog::format_line(e);
    CHECK(line ==
          "2026-05-20T12:34:56Z\tTopicChanged\t1\tch\\tat\tline1\\nline2\\\\done\n");
}

TEST_CASE("parse_line round-trips format_line for tricky strings",
          "[infra][audit][file]") {
    auto e = make_entry(ap::AuditAction::ChannelMemberKicked, 7,
                        "name\twith\ttabs", "msg\nwith\nnewlines");
    auto line = FileAuditLog::format_line(e);

    ap::AuditEntry parsed{};
    REQUIRE(FileAuditLog::parse_line(line, parsed));
    CHECK(parsed.action  == e.action);
    CHECK(parsed.actor   == e.actor);
    CHECK(parsed.subject == e.subject);
    CHECK(parsed.details == e.details);
    CHECK(parsed.timestamp == e.timestamp);
}

TEST_CASE("parse_line rejects malformed input", "[infra][audit][file]") {
    ap::AuditEntry out{};
    CHECK_FALSE(FileAuditLog::parse_line("only-two\tfields", out));
    CHECK_FALSE(FileAuditLog::parse_line(
        "not-a-date\tAccountBanned\t1\ts\td", out));
    CHECK_FALSE(FileAuditLog::parse_line(
        "2026-05-20T12:34:56Z\tBogusAction\t1\ts\td", out));
    CHECK_FALSE(FileAuditLog::parse_line(
        "2026-05-20T12:34:56Z\tAccountBanned\tNAN\ts\td", out));
}

TEST_CASE("file audit log persists and replays entries",
          "[infra][audit][file]") {
    auto path = tmp_path("rw");

    {
        auto log = FileAuditLog::try_open(path);
        REQUIRE(log);
        log->record(make_entry(ap::AuditAction::AccountCreated, 1, "alice", ""));
        log->record(make_entry(ap::AuditAction::AccountBanned, 2, "bob", "spam"));
        log->record(make_entry(ap::AuditAction::ServerShutdown, 0, "", "ok"));
    }
    {
        auto log = FileAuditLog::try_open(path);
        REQUIRE(log);
        auto last2 = log->recent(2);
        REQUIRE(last2.size() == 2);
        CHECK(last2[0].action == ap::AuditAction::AccountBanned);
        CHECK(last2[0].subject == "bob");
        CHECK(last2[1].action == ap::AuditAction::ServerShutdown);
        CHECK(last2[1].details == "ok");

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
