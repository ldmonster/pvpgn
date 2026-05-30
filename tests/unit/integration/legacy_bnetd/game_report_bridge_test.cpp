// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for the observation-only game-report bridge:
// `pvpgn_v3_gamereport`. Always returns 0; emits a single
// structured-log line at Debug level with `{user, player_count}`.

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"
#include "integration/legacy_bnetd/game_report_bridge.hpp"

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

}  // namespace

TEST_CASE("gamereport bridge null-conn is no-op",
          "[integration][legacy_bnetd][game_report_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_gamereport(nullptr, "user", 8u) == 0);
    REQUIRE(sink.records.empty());
}

TEST_CASE("gamereport bridge logs structured fields",
          "[integration][legacy_bnetd][game_report_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    int marker = 0;
    REQUIRE(::pvpgn_v3_gamereport(&marker, "alice", 4u) == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.module == "v3_game_report_bridge");
    REQUIRE(r.message == "gamereport intent observed");
    REQUIRE(r.fields.size() == 2u);
    REQUIRE(r.fields[0].first == "user");
    REQUIRE(r.fields[0].second == "alice");
    REQUIRE(r.fields[1].first == "player_count");
    REQUIRE(r.fields[1].second == "4");
}

TEST_CASE("gamereport bridge null username -> empty",
          "[integration][legacy_bnetd][game_report_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    int marker = 0;
    REQUIRE(::pvpgn_v3_gamereport(&marker, nullptr, 0u) == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(sink.records[0].fields[0].second.empty());
    REQUIRE(sink.records[0].fields[1].second == "0");
}

TEST_CASE("gamereport bridge large player count",
          "[integration][legacy_bnetd][game_report_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    int marker = 0;
    REQUIRE(::pvpgn_v3_gamereport(&marker, "u", 4294967295u) == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(sink.records[0].fields[1].second == "4294967295");
}
