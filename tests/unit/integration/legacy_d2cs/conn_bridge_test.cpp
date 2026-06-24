// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for the d2cs conn-destroy observation bridge.

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>

#include "core/logging.hpp"
#include "app/d2cs/legacy_d2cs_bridges/bridge_logger.hpp"
#include "app/d2cs/legacy_d2cs_bridges/conn_bridge.hpp"

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

TEST_CASE("d2cs conn-destroy bridge logs scalars",
          "[integration][legacy_d2cs][conn_bridge]") {
    RecordingLogger sink;
    ilc::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_conn_destroy(11, 4242u, 1u, 3u) == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Debug);
    REQUIRE(r.module  == "v3_d2cs_conn_bridge");
    REQUIRE(r.message == "conn destroy observed");
    REQUIRE(r.fields.size() == 4u);
    REQUIRE(field_value(r, "sd")         == "11");
    REQUIRE(field_value(r, "sessionnum") == "4242");
    REQUIRE(field_value(r, "cclass")     == "1");
    REQUIRE(field_value(r, "state")      == "3");
}

TEST_CASE("d2cs conn-destroy bridge handles negative sd",
          "[integration][legacy_d2cs][conn_bridge]") {
    RecordingLogger sink;
    ilc::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_conn_destroy(-1, 0u, 0u, 0u) == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(field_value(r, "sd") == "-1");
}

TEST_CASE("d2cs conn-destroy bridge override clears on guard destruction",
          "[integration][legacy_d2cs][conn_bridge]") {
    RecordingLogger sink;
    {
        ilc::BridgeLoggerOverride guard(sink);
        REQUIRE(::pvpgn_v3_d2cs_conn_destroy(1, 1u, 1u, 1u) == 0);
    }
    const std::size_t before = sink.records.size();
    REQUIRE(::pvpgn_v3_d2cs_conn_destroy(2, 2u, 2u, 2u) == 0);
    REQUIRE(sink.records.size() == before);
}
