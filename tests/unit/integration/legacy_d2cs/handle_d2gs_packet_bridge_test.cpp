// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for the d2cs d2gs-packet-dispatcher observation bridge.

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>

#include "core/logging.hpp"
#include "app/d2cs/legacy_d2cs_bridges/bridge_logger.hpp"
#include "app/d2cs/legacy_d2cs_bridges/handle_d2gs_packet_bridge.hpp"

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

TEST_CASE("d2cs d2gs-packet bridge logs scalars",
          "[integration][legacy_d2cs][handle_d2gs_packet_bridge]") {
    RecordingLogger sink;
    ilc::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_handle_d2gs_packet(33, 0x42u, 256u) == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.module  == "v3_d2cs_handle_d2gs_bridge");
    REQUIRE(r.message == "d2gs packet dispatched");
    REQUIRE(field_value(r, "sd")          == "33");
    REQUIRE(field_value(r, "packet_type") == "66");
    REQUIRE(field_value(r, "packet_size") == "256");
}

TEST_CASE("d2cs d2gs-packet bridge override clears on guard destruction",
          "[integration][legacy_d2cs][handle_d2gs_packet_bridge]") {
    RecordingLogger sink;
    {
        ilc::BridgeLoggerOverride guard(sink);
        REQUIRE(::pvpgn_v3_d2cs_handle_d2gs_packet(1, 1u, 1u) == 0);
    }
    const std::size_t before = sink.records.size();
    REQUIRE(::pvpgn_v3_d2cs_handle_d2gs_packet(2, 2u, 2u) == 0);
    REQUIRE(sink.records.size() == before);
}
