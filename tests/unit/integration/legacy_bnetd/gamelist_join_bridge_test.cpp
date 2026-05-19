// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for the observation-only gamelist/joingame bridges.

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"
#include "integration/legacy_bnetd/gamelist_join_bridge.hpp"

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

TEST_CASE("gamelistreq bridge null-conn is no-op",
          "[integration][legacy_bnetd][gamelist_join_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_gamelistreq_try(nullptr, "g", 0u) == 0);
    REQUIRE(sink.records.empty());
}

TEST_CASE("gamelistreq bridge public_list scope",
          "[integration][legacy_bnetd][gamelist_join_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    int marker = 0;
    REQUIRE(::pvpgn_v3_gamelistreq_try(&marker, "", 0x0a) == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.module == "v3_gamelist_join_bridge");
    REQUIRE(r.message == "gamelistreq intent observed");
    REQUIRE(field_value(r, "scope")   == "public_list");
    REQUIRE(field_value(r, "game").empty());
    REQUIRE(field_value(r, "bngtype") == "0x0000000a");
}

TEST_CASE("gamelistreq bridge specific scope",
          "[integration][legacy_bnetd][gamelist_join_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    int marker = 0;
    REQUIRE(::pvpgn_v3_gamelistreq_try(&marker, "MyGame", 0x4) == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(field_value(sink.records[0], "scope") == "specific");
    REQUIRE(field_value(sink.records[0], "game")  == "MyGame");
}

TEST_CASE("gamelistreq bridge null gamename -> public_list",
          "[integration][legacy_bnetd][gamelist_join_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    int marker = 0;
    REQUIRE(::pvpgn_v3_gamelistreq_try(&marker, nullptr, 0u) == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(field_value(sink.records[0], "scope") == "public_list");
}

TEST_CASE("joingame bridge null-conn is no-op",
          "[integration][legacy_bnetd][gamelist_join_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_joingame_try(nullptr, "g") == 0);
    REQUIRE(sink.records.empty());
}

TEST_CASE("joingame bridge happy path",
          "[integration][legacy_bnetd][gamelist_join_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    int marker = 0;
    REQUIRE(::pvpgn_v3_joingame_try(&marker, "BNet") == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.module == "v3_gamelist_join_bridge");
    REQUIRE(r.message == "joingame intent observed");
    REQUIRE(field_value(r, "game") == "BNet");
}

TEST_CASE("joingame bridge null gamename",
          "[integration][legacy_bnetd][gamelist_join_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    int marker = 0;
    REQUIRE(::pvpgn_v3_joingame_try(&marker, nullptr) == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(field_value(sink.records[0], "game").empty());
}
