// SPDX-License-Identifier: GPL-2.0-or-later
//
// Batch 24c: composition-root smoke for the "ILogger seam plus the
// JsonLineLogger sink" combination. The bnetd composition root
// (`server_process()`) reads PVPGN_LOG_FORMAT at startup and installs
// either `LegacyEventLogger` or `JsonLineLogger`; this test pins the
// "set_default_logger(JsonLineLogger) then call core::log(...)"
// pipeline end-to-end so future refactors of either side cannot
// silently break the wiring.

#include <atomic>
#include <memory>
#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "core/logging.hpp"
#include "infra/log/json_line_logger.hpp"

using pvpgn::core::LogLevel;
using pvpgn::infra::log::JsonLineLogger;

namespace {

// RAII guard around set_default_logger so the global atom is
// restored after each TEST_CASE regardless of REQUIRE outcome.
class DefaultLoggerScope {
public:
    explicit DefaultLoggerScope(std::shared_ptr<pvpgn::core::ILogger> s) {
        pvpgn::core::set_default_logger(std::move(s));
    }
    ~DefaultLoggerScope() {
        pvpgn::core::set_default_logger(nullptr);
    }
    DefaultLoggerScope(const DefaultLoggerScope&)            = delete;
    DefaultLoggerScope& operator=(const DefaultLoggerScope&) = delete;
};

}  // namespace

TEST_CASE("composition-root: set_default_logger(JsonLineLogger) routes "
          "core::log() to NDJSON",
          "[infra][log][json][composition]") {
    auto sink = std::make_shared<std::ostringstream>();
    auto logger = std::make_shared<JsonLineLogger>(
        *sink, LogLevel::Trace, [] { return std::int64_t{99}; });

    DefaultLoggerScope guard{logger};

    // Any call to `core::default_logger().log(...)` should reach the
    // JsonLineLogger sink and produce exactly one NDJSON record.
    pvpgn::core::default_logger().log(
        LogLevel::Warn, "v3_smoke", "hello composition root");

    auto out = sink->str();
    REQUIRE(!out.empty());
    REQUIRE(out.find(R"("lvl":"warn")") != std::string::npos);
    REQUIRE(out.find(R"("mod":"v3_smoke")") != std::string::npos);
    REQUIRE(out.find(R"("msg":"hello composition root")")
            != std::string::npos);
    REQUIRE(out.find(R"("ts":99)") != std::string::npos);
    // Exactly one line.
    std::size_t newlines = 0;
    for (char c : out) if (c == '\n') ++newlines;
    REQUIRE(newlines == 1);
}

TEST_CASE("composition-root: clearing the default reverts to NullLogger",
          "[infra][log][json][composition]") {
    {
        auto sink = std::make_shared<std::ostringstream>();
        auto logger = std::make_shared<JsonLineLogger>(
            *sink, LogLevel::Trace, [] { return std::int64_t{0}; });
        DefaultLoggerScope guard{logger};
        pvpgn::core::default_logger().log(
            LogLevel::Info, "m", "while-installed");
        REQUIRE_FALSE(sink->str().empty());
    }
    // After the guard's dtor: the default has been reset to nullptr,
    // which the core resolves to a `NullLogger`. Calling log() must
    // not throw and must not affect any previously-held sink.
    pvpgn::core::default_logger().log(
        LogLevel::Error, "m", "after-clear");
    SUCCEED("default_logger() remains callable after reset");
}
