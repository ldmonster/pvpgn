// SPDX-License-Identifier: GPL-2.0-or-later
// R243 -- unit tests for bnetd timer subsystem observation bridges.

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"
#include "integration/legacy_bnetd/timer_bridge.hpp"

namespace ila = pvpgn::integration::legacy_bnetd;

namespace {

struct CapturedLog {
    pvpgn::core::LogLevel level{};
    std::string module;
    std::string message;
    std::vector<std::pair<std::string, std::string>> fields;
};

class RecordingLogger : public pvpgn::core::ILogger {
public:
    void log(pvpgn::core::LogLevel level,
             std::string_view module,
             std::string_view message) noexcept override {
        records.push_back(CapturedLog{
            level, std::string{module}, std::string{message}, {}});
    }
    pvpgn::core::LogLevel level() const noexcept override {
        return pvpgn::core::LogLevel::Trace;
    }
    void set_level(pvpgn::core::LogLevel) noexcept override {}
    void log_kv(pvpgn::core::LogLevel level,
                std::string_view module,
                std::string_view message,
                std::span<const Field> fields) noexcept override {
        CapturedLog c;
        c.level   = level;
        c.module  = std::string{module};
        c.message = std::string{message};
        for (const auto& f : fields) {
            c.fields.emplace_back(std::string{f.key},
                                  std::string{f.value});
        }
        records.push_back(std::move(c));
    }
    std::vector<CapturedLog> records;
};

std::string field_value(const CapturedLog& r,
                        std::string_view key) {
    for (const auto& kv : r.fields) {
        if (kv.first == key) return kv.second;
    }
    return {};
}

}  // namespace

TEST_CASE("timer create bridge logs at Debug",
          "[integration][legacy_bnetd][timer_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_timerlist_create() == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Debug);
    REQUIRE(r.module  == "v3_bnetd_timer_bridge");
    REQUIRE(r.message == "timerlist create observed");
    REQUIRE(r.fields.empty());
}

TEST_CASE("timer destroy bridge logs at Debug",
          "[integration][legacy_bnetd][timer_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_timerlist_destroy() == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Debug);
    REQUIRE(r.module  == "v3_bnetd_timer_bridge");
    REQUIRE(r.message == "timerlist destroy observed");
}

TEST_CASE("timer add_timer bridge records sd and when",
          "[integration][legacy_bnetd][timer_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_timerlist_add_timer(7, 1700000000ULL) == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Trace);
    REQUIRE(r.module  == "v3_bnetd_timer_bridge");
    REQUIRE(r.message == "timerlist add_timer observed");
    REQUIRE(std::string{field_value(r, "sd")}   == "7");
    REQUIRE(std::string{field_value(r, "when")} == "1700000000");
}

TEST_CASE("timer del_all_timers bridge records sd",
          "[integration][legacy_bnetd][timer_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_timerlist_del_all_timers(42) == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Debug);
    REQUIRE(r.module  == "v3_bnetd_timer_bridge");
    REQUIRE(r.message == "timerlist del_all_timers observed");
    REQUIRE(std::string{field_value(r, "sd")} == "42");
}

TEST_CASE("timer check_timers bridge records when",
          "[integration][legacy_bnetd][timer_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_timerlist_check_timers(1234567890ULL) == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Trace);
    REQUIRE(r.module  == "v3_bnetd_timer_bridge");
    REQUIRE(r.message == "timerlist check_timers observed");
    REQUIRE(std::string{field_value(r, "when")} == "1234567890");
}
