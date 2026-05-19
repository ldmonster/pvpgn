// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for the coalesced anongame_infos runtime getter
// observation bridge.

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/anongame_infos_get_bridge.hpp"
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

TEST_CASE("anongame_infos get bridge logs kind+args",
          "[integration][legacy_bnetd][anongame_infos_get_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_anongame_infos_get_try(
        "URL_get_URL", "3", "0", nullptr) == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.module  == "v3_anongame_infos_get_bridge");
    REQUIRE(r.message == "anongame_infos get observed");
    REQUIRE(field_value(r, "kind") == "URL_get_URL");
    REQUIRE(field_value(r, "arg0") == "3");
    REQUIRE(field_value(r, "arg1") == "0");
    REQUIRE(field_value(r, "arg2").empty());
}

TEST_CASE("anongame_infos get bridge full args",
          "[integration][legacy_bnetd][anongame_infos_get_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_anongame_infos_get_try(
        "data_get_map", "5", "0", "WAR3") == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(field_value(sink.records[0], "kind") == "data_get_map");
    REQUIRE(field_value(sink.records[0], "arg2") == "WAR3");
}

TEST_CASE("anongame_infos get bridge null kind becomes ?",
          "[integration][legacy_bnetd][anongame_infos_get_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_anongame_infos_get_try(
        nullptr, nullptr, nullptr, nullptr) == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(field_value(sink.records[0], "kind") == "?");
    REQUIRE(field_value(sink.records[0], "arg0").empty());
}
