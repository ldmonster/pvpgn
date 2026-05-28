// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for the d2cs init-packet observation bridge (R234).

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>

#include "core/logging.hpp"
#include "integration/legacy_d2cs/bridge_logger.hpp"
#include "integration/legacy_d2cs/handle_init_bridge.hpp"

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

TEST_CASE("d2cs init-packet bridge logs scalars",
          "[integration][legacy_d2cs][handle_init_bridge]") {
    RecordingLogger sink;
    ilc::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_handle_init_packet_try(7, 1u) == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Trace);
    REQUIRE(r.module  == "v3_d2cs_handle_init_bridge");
    REQUIRE(r.message == "init packet observed");
    REQUIRE(r.fields.size() == 2u);
    REQUIRE(field_value(r, "sd")     == "7");
    REQUIRE(field_value(r, "cclass") == "1");
}

TEST_CASE("d2cs init-packet bridge renders d2gs class id",
          "[integration][legacy_d2cs][handle_init_bridge]") {
    RecordingLogger sink;
    ilc::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_handle_init_packet_try(42, 2u) == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(field_value(r, "sd")     == "42");
    REQUIRE(field_value(r, "cclass") == "2");
}

TEST_CASE("d2cs init-packet bridge override clears on guard destruction",
          "[integration][legacy_d2cs][handle_init_bridge]") {
    RecordingLogger sink;
    {
        ilc::BridgeLoggerOverride guard(sink);
        REQUIRE(::pvpgn_v3_d2cs_handle_init_packet_try(1, 1u) == 0);
    }
    const std::size_t before = sink.records.size();
    REQUIRE(::pvpgn_v3_d2cs_handle_init_packet_try(2, 2u) == 0);
    REQUIRE(sink.records.size() == before);
}

TEST_CASE("d2cs on_d2gs_initconn bridge logs at info",
          "[integration][legacy_d2cs][handle_init_bridge]") {
    RecordingLogger sink;
    ilc::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_on_d2gs_initconn_try(11, 0x7F000001u) == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Info);
    REQUIRE(r.module  == "v3_d2cs_handle_init_bridge");
    REQUIRE(r.message == "on d2gs initconn observed");
    REQUIRE(r.fields.size() == 2u);
    REQUIRE(field_value(r, "sd")   == "11");
    REQUIRE(field_value(r, "addr") == "2130706433");
}

TEST_CASE("d2cs on_d2cs_initconn bridge logs at info",
          "[integration][legacy_d2cs][handle_init_bridge]") {
    RecordingLogger sink;
    ilc::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_on_d2cs_initconn_try(13) == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Info);
    REQUIRE(r.module  == "v3_d2cs_handle_init_bridge");
    REQUIRE(r.message == "on d2cs initconn observed");
    REQUIRE(r.fields.size() == 1u);
    REQUIRE(field_value(r, "sd") == "13");
}
