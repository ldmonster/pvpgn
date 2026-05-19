// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for the observation-only anongame_infos lifecycle
// bridge. Unlike the other connection-keyed bridges, the load /
// unload pair fires from server bootstrap / shutdown paths and
// has no `conn_ptr` parameter; therefore every call MUST log
// regardless of state.

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/anongame_infos_bridge.hpp"
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

TEST_CASE("anongame_infos load bridge logs file",
          "[integration][legacy_bnetd][anongame_infos_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_anongame_infos_load_try(
        "/etc/pvpgn/anongame_infos.conf") == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.module  == "v3_anongame_infos_bridge");
    REQUIRE(r.message == "anongame_infos load observed");
    REQUIRE(field_value(r, "file") == "/etc/pvpgn/anongame_infos.conf");
}

TEST_CASE("anongame_infos load bridge null filename",
          "[integration][legacy_bnetd][anongame_infos_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_anongame_infos_load_try(nullptr) == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(field_value(sink.records[0], "file").empty());
}

TEST_CASE("anongame_infos unload bridge logs",
          "[integration][legacy_bnetd][anongame_infos_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_anongame_infos_unload_try() == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.module  == "v3_anongame_infos_bridge");
    REQUIRE(r.message == "anongame_infos unload observed");
    REQUIRE(r.fields.empty());
}
