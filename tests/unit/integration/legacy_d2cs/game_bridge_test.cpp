// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for the d2cs game catalogue + per-game lifecycle bridges (R238).

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>

#include "core/logging.hpp"
#include "integration/legacy_d2cs/bridge_logger.hpp"
#include "integration/legacy_d2cs/game_bridge.hpp"

namespace ilc = pvpgn::integration::legacy_d2cs;

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
    return std::string{};
}

}  // namespace

TEST_CASE("d2cs gamelist create bridge logs at debug",
          "[integration][legacy_d2cs][game_bridge]") {
    RecordingLogger sink;
    ilc::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_gamelist_create() == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Debug);
    REQUIRE(r.module  == "v3_d2cs_game_bridge");
    REQUIRE(r.message == "gamelist create observed");
    REQUIRE(r.fields.empty());
}

TEST_CASE("d2cs gamelist destroy bridge logs at debug",
          "[integration][legacy_d2cs][game_bridge]") {
    RecordingLogger sink;
    ilc::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_gamelist_destroy() == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(sink.records.front().message == "gamelist destroy observed");
}

TEST_CASE("d2cs game create bridge logs id + name + flag at info",
          "[integration][legacy_d2cs][game_bridge]") {
    RecordingLogger sink;
    ilc::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_game_create(17u, "test-game", 0x10u) == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Info);
    REQUIRE(r.module  == "v3_d2cs_game_bridge");
    REQUIRE(r.message == "game create observed");
    REQUIRE(r.fields.size() == 3u);
    REQUIRE(field_value(r, "id")       == "17");
    REQUIRE(field_value(r, "gamename") == "test-game");
    REQUIRE(field_value(r, "gameflag") == "16");
}

TEST_CASE("d2cs game create bridge handles null name",
          "[integration][legacy_d2cs][game_bridge]") {
    RecordingLogger sink;
    ilc::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_game_create(1u, nullptr, 0u) == 0);
    REQUIRE(field_value(sink.records.front(), "gamename") == "<null>");
}

TEST_CASE("d2cs game destroy bridge logs id + name at info",
          "[integration][legacy_d2cs][game_bridge]") {
    RecordingLogger sink;
    ilc::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_game_destroy(42u, "doomed") == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Info);
    REQUIRE(r.message == "game destroy observed");
    REQUIRE(field_value(r, "id")       == "42");
    REQUIRE(field_value(r, "gamename") == "doomed");
}

TEST_CASE("d2cs game destroy bridge handles null name",
          "[integration][legacy_d2cs][game_bridge]") {
    RecordingLogger sink;
    ilc::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_game_destroy(99u, nullptr) == 0);
    REQUIRE(field_value(sink.records.front(), "gamename") == "<null>");
}

TEST_CASE("d2cs game bridge override clears on guard destruction",
          "[integration][legacy_d2cs][game_bridge]") {
    RecordingLogger sink;
    {
        ilc::BridgeLoggerOverride guard(sink);
        REQUIRE(::pvpgn_v3_d2cs_gamelist_create() == 0);
    }
    const std::size_t before = sink.records.size();
    REQUIRE(::pvpgn_v3_d2cs_gamelist_destroy() == 0);
    REQUIRE(sink.records.size() == before);
}

TEST_CASE("d2cs game_set_d2gs_gameid bridge logs at debug",
          "[integration][legacy_d2cs][game_bridge]") {
    RecordingLogger sink;
    ilc::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_game_set_d2gs_gameid(7u, 12345u) == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Debug);
    REQUIRE(r.module  == "v3_d2cs_game_bridge");
    REQUIRE(r.message == "game set d2gs_gameid observed");
    REQUIRE(field_value(r, "game_id")     == "7");
    REQUIRE(field_value(r, "d2gs_gameid") == "12345");
}

TEST_CASE("d2cs game_set_d2gs bridge logs at debug with d2gs id",
          "[integration][legacy_d2cs][game_bridge]") {
    RecordingLogger sink;
    ilc::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_game_set_d2gs(7u, 3u) == 0);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Debug);
    REQUIRE(r.message == "game set d2gs observed");
    REQUIRE(field_value(r, "game_id") == "7");
    REQUIRE(field_value(r, "d2gs_id") == "3");
}

TEST_CASE("d2cs game_set_d2gs bridge logs zero d2gs_id when detaching",
          "[integration][legacy_d2cs][game_bridge]") {
    RecordingLogger sink;
    ilc::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_game_set_d2gs(7u, 0u) == 0);
    REQUIRE(field_value(sink.records.front(), "d2gs_id") == "0");
}

TEST_CASE("d2cs game_set_created bridge logs at info",
          "[integration][legacy_d2cs][game_bridge]") {
    RecordingLogger sink;
    ilc::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_game_set_created(7u, 1u) == 0);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Info);
    REQUIRE(r.message == "game set created observed");
    REQUIRE(field_value(r, "game_id") == "7");
    REQUIRE(field_value(r, "created") == "1");
}

TEST_CASE("d2cs game_add_character bridge logs scalars + name at info",
          "[integration][legacy_d2cs][game_bridge]") {
    RecordingLogger sink;
    ilc::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_game_add_character(7u, "ash", 3u, 42u) == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Info);
    REQUIRE(r.module  == "v3_d2cs_game_bridge");
    REQUIRE(r.message == "game add character observed");
    REQUIRE(r.fields.size() == 4u);
    REQUIRE(field_value(r, "game_id")  == "7");
    REQUIRE(field_value(r, "charname") == "ash");
    REQUIRE(field_value(r, "chclass")  == "3");
    REQUIRE(field_value(r, "level")    == "42");
}

TEST_CASE("d2cs game_add_character bridge handles null name",
          "[integration][legacy_d2cs][game_bridge]") {
    RecordingLogger sink;
    ilc::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_game_add_character(1u, nullptr, 0u, 1u) == 0);
    REQUIRE(field_value(sink.records.front(), "charname") == "<null>");
}

TEST_CASE("d2cs game_del_character bridge logs game_id + name at info",
          "[integration][legacy_d2cs][game_bridge]") {
    RecordingLogger sink;
    ilc::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_game_del_character(7u, "ash") == 0);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Info);
    REQUIRE(r.message == "game del character observed");
    REQUIRE(field_value(r, "game_id")  == "7");
    REQUIRE(field_value(r, "charname") == "ash");
}

TEST_CASE("d2cs game_del_character bridge handles null name",
          "[integration][legacy_d2cs][game_bridge]") {
    RecordingLogger sink;
    ilc::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_game_del_character(1u, nullptr) == 0);
    REQUIRE(field_value(sink.records.front(), "charname") == "<null>");
}
