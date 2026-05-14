// SPDX-License-Identifier: GPL-2.0-or-later
#include <span>
#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "infra/log/json_line_logger.hpp"

using pvpgn::core::LogLevel;
using pvpgn::infra::log::JsonLineLogger;

namespace {

std::string trim_nl(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    return s;
}

}  // namespace

TEST_CASE("json_line_logger: emits single NDJSON record per call",
          "[infra][log][json]") {
    std::ostringstream oss;
    JsonLineLogger lg(oss, LogLevel::Trace, [] { return std::int64_t{42}; });
    lg.log(LogLevel::Info, "boot", "started ok");
    auto out = trim_nl(oss.str());
    REQUIRE(out ==
            R"({"ts":42,"lvl":"info","mod":"boot","msg":"started ok"})");
}

TEST_CASE("json_line_logger: drops records below min level",
          "[infra][log][json]") {
    std::ostringstream oss;
    JsonLineLogger lg(oss, LogLevel::Warn, [] { return std::int64_t{0}; });
    lg.log(LogLevel::Info, "x", "below");
    REQUIRE(oss.str().empty());
    lg.log(LogLevel::Warn, "x", "above");
    REQUIRE_FALSE(oss.str().empty());
}

TEST_CASE("json_line_logger: escapes JSON special characters",
          "[infra][log][json]") {
    std::ostringstream oss;
    JsonLineLogger lg(oss, LogLevel::Trace, [] { return std::int64_t{0}; });
    lg.log(LogLevel::Info, "m", "quote=\" backslash=\\ newline=\n tab=\t");
    auto out = trim_nl(oss.str());
    REQUIRE(out.find(R"(\")") != std::string::npos);
    REQUIRE(out.find(R"(\\)") != std::string::npos);
    REQUIRE(out.find(R"(\n)") != std::string::npos);
    REQUIRE(out.find(R"(\t)") != std::string::npos);
    // No bare control character in the output.
    for (char c : out) {
        REQUIRE(static_cast<unsigned char>(c) >= 0x20);
    }
}

TEST_CASE("json_line_logger: emits one record per line for multiple calls",
          "[infra][log][json]") {
    std::ostringstream oss;
    JsonLineLogger lg(oss, LogLevel::Trace, [] { return std::int64_t{7}; });
    lg.log(LogLevel::Info, "a", "one");
    lg.log(LogLevel::Warn, "b", "two");
    auto out = oss.str();
    // exactly two trailing newlines, no embedded.
    std::size_t nls = 0;
    for (char c : out) if (c == '\n') ++nls;
    REQUIRE(nls == 2);
}

TEST_CASE("json_line_logger: escapes control-byte 0x01 as \\u0001",
          "[infra][log][json]") {
    std::ostringstream oss;
    JsonLineLogger lg(oss, LogLevel::Trace, [] { return std::int64_t{0}; });
    std::string body;
    body.push_back('\x01');
    lg.log(LogLevel::Info, "m", body);
    auto out = trim_nl(oss.str());
    REQUIRE(out.find(R"(\u0001)") != std::string::npos);
}

TEST_CASE("json_line_logger: emits structured fields after msg",
          "[infra][log][json][kv]") {
    using pvpgn::core::ILogger;
    std::ostringstream oss;
    JsonLineLogger lg(oss, LogLevel::Trace, [] { return std::int64_t{42}; });
    const ILogger::Field fields[] = {
        {"account_id", "1234"},
        {"event",      "login_succeeded"},
    };
    lg.log_kv(LogLevel::Info, "auth", "user authed",
              std::span<const ILogger::Field>{fields});
    auto out = trim_nl(oss.str());
    REQUIRE(out
            == R"({"ts":42,"lvl":"info","mod":"auth","msg":"user authed","account_id":"1234","event":"login_succeeded"})");
}

TEST_CASE("json_line_logger: escapes field keys + values",
          "[infra][log][json][kv]") {
    using pvpgn::core::ILogger;
    std::ostringstream oss;
    JsonLineLogger lg(oss, LogLevel::Trace, [] { return std::int64_t{0}; });
    const ILogger::Field fields[] = {
        {"k\"ey", "va\\lue"},
    };
    lg.log_kv(LogLevel::Info, "m", "x",
              std::span<const ILogger::Field>{fields});
    auto out = trim_nl(oss.str());
    REQUIRE(out.find(R"("k\"ey":"va\\lue")") != std::string::npos);
}
