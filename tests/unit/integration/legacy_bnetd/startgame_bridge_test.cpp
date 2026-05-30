// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for the observation-only startgame bridge.

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"
#include "integration/legacy_bnetd/startgame_bridge.hpp"

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

TEST_CASE("startgame bridge null-conn is no-op",
          "[integration][legacy_bnetd][startgame_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_startgame(nullptr, 1u, "g", "i",
                                     0u, 0u, 0u, 0u) == 0);
    REQUIRE(sink.records.empty());
}

TEST_CASE("startgame bridge variant names",
          "[integration][legacy_bnetd][startgame_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    int marker = 0;
    REQUIRE(::pvpgn_v3_startgame(&marker, 1u, "a", "",
                                     0x4, 0x0, 0, 0) == 0);
    REQUIRE(::pvpgn_v3_startgame(&marker, 3u, "b", "",
                                     0x4, 0x0, 0, 0) == 0);
    REQUIRE(::pvpgn_v3_startgame(&marker, 4u, "c", "",
                                     0x4, 0x0, 0, 0) == 0);
    REQUIRE(::pvpgn_v3_startgame(&marker, 99u, "d", "",
                                     0x4, 0x0, 0, 0) == 0);
    REQUIRE(sink.records.size() == 4u);
    REQUIRE(field_value(sink.records[0], "variant") == "STARTGAME1");
    REQUIRE(field_value(sink.records[1], "variant") == "STARTGAME3");
    REQUIRE(field_value(sink.records[2], "variant") == "STARTGAME4");
    REQUIRE(field_value(sink.records[3], "variant") == "?");
    REQUIRE(field_value(sink.records[3], "version") == "99");
}

TEST_CASE("startgame bridge hex formatting",
          "[integration][legacy_bnetd][startgame_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    int marker = 0;
    REQUIRE(::pvpgn_v3_startgame(&marker, 4u, "mygame", "info123",
                                     0x000bu, 0x00000003u,
                                     0x00cdu, 0x0040u) == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.module == "v3_startgame_bridge");
    REQUIRE(r.message == "startgame intent observed");
    REQUIRE(field_value(r, "game")    == "mygame");
    REQUIRE(field_value(r, "bngtype") == "0x0000000b");
    REQUIRE(field_value(r, "status")  == "0x00000003");
    REQUIRE(field_value(r, "flag")    == "0x000000cd");
    REQUIRE(field_value(r, "option")  == "0x00000040");
    REQUIRE(field_value(r, "infolen") == "7");
}

TEST_CASE("startgame bridge null strings",
          "[integration][legacy_bnetd][startgame_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    int marker = 0;
    REQUIRE(::pvpgn_v3_startgame(&marker, 1u, nullptr, nullptr,
                                     0u, 0u, 0u, 0u) == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(field_value(sink.records[0], "game").empty());
    REQUIRE(field_value(sink.records[0], "infolen") == "0");
}
