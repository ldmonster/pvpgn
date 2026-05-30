// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for the observation-only anongame dispatch bridge.

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/anongame_dispatch_bridge.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"

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

TEST_CASE("anongame dispatch bridge null-conn is no-op",
          "[integration][legacy_bnetd][anongame_dispatch_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_anongame_dispatch(nullptr, 0x00) == 0);
    REQUIRE(sink.records.empty());
}

TEST_CASE("anongame dispatch bridge option name table",
          "[integration][legacy_bnetd][anongame_dispatch_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    int marker = 0;
    REQUIRE(::pvpgn_v3_anongame_dispatch(&marker, 0x00) == 0);
    REQUIRE(::pvpgn_v3_anongame_dispatch(&marker, 0x02) == 0);
    REQUIRE(::pvpgn_v3_anongame_dispatch(&marker, 0x03) == 0);
    REQUIRE(::pvpgn_v3_anongame_dispatch(&marker, 0x04) == 0);
    REQUIRE(::pvpgn_v3_anongame_dispatch(&marker, 0x05) == 0);
    REQUIRE(::pvpgn_v3_anongame_dispatch(&marker, 0x06) == 0);
    REQUIRE(::pvpgn_v3_anongame_dispatch(&marker, 0x07) == 0);
    REQUIRE(::pvpgn_v3_anongame_dispatch(&marker, 0x08) == 0);
    REQUIRE(::pvpgn_v3_anongame_dispatch(&marker, 0x09) == 0);
    REQUIRE(::pvpgn_v3_anongame_dispatch(&marker, 0x0a) == 0);
    REQUIRE(::pvpgn_v3_anongame_dispatch(&marker, 0xff) == 0);
    REQUIRE(sink.records.size() == 11u);
    REQUIRE(field_value(sink.records[0],  "option") == "SEARCH");
    REQUIRE(field_value(sink.records[1],  "option") == "INFOS");
    REQUIRE(field_value(sink.records[2],  "option") == "CANCEL");
    REQUIRE(field_value(sink.records[3],  "option") == "PROFILE");
    REQUIRE(field_value(sink.records[4],  "option") == "AT_SEARCH");
    REQUIRE(field_value(sink.records[5],  "option") == "AT_INVITER_SEARCH");
    REQUIRE(field_value(sink.records[6],  "option") == "TOURNAMENT");
    REQUIRE(field_value(sink.records[7],  "option") == "PROFILE_CLAN");
    REQUIRE(field_value(sink.records[8],  "option") == "GET_ICON");
    REQUIRE(field_value(sink.records[9],  "option") == "SET_ICON");
    REQUIRE(field_value(sink.records[10], "option") == "?");
    REQUIRE(field_value(sink.records[10], "raw")    == "0xff");
}

TEST_CASE("anongame dispatch bridge hex raw value",
          "[integration][legacy_bnetd][anongame_dispatch_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    int marker = 0;
    REQUIRE(::pvpgn_v3_anongame_dispatch(&marker, 0x04) == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.module == "v3_anongame_dispatch_bridge");
    REQUIRE(r.message == "anongame dispatch observed");
    REQUIRE(field_value(r, "raw") == "0x04");
}
