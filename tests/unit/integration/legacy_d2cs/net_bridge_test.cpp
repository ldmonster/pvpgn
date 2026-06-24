// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for the d2cs net.cpp socket-helper observation bridges.

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>

#include "core/logging.hpp"
#include "app/d2cs/legacy_d2cs_bridges/bridge_logger.hpp"
#include "app/d2cs/legacy_d2cs_bridges/net_bridge.hpp"

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

TEST_CASE("d2cs net_socket bridge logs type at debug",
          "[integration][legacy_d2cs][net_bridge]") {
    RecordingLogger sink;
    ilc::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_net_socket(1) == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Debug);
    REQUIRE(r.module  == "v3_d2cs_net_bridge");
    REQUIRE(r.message == "net socket observed");
    REQUIRE(field_value(r, "type") == "1");
}

TEST_CASE("d2cs net_check_connected bridge logs sock at trace",
          "[integration][legacy_d2cs][net_bridge]") {
    RecordingLogger sink;
    ilc::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_net_check_connected(11) == 0);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Trace);
    REQUIRE(r.message == "net check_connected observed");
    REQUIRE(field_value(r, "sock") == "11");
}

TEST_CASE("d2cs net_listen bridge logs ip/port/type at info",
          "[integration][legacy_d2cs][net_bridge]") {
    RecordingLogger sink;
    ilc::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_net_listen(0u, 6113u, 1) == 0);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Info);
    REQUIRE(r.message == "net listen observed");
    REQUIRE(r.fields.size() == 3u);
    REQUIRE(field_value(r, "ip")   == "0");
    REQUIRE(field_value(r, "port") == "6113");
    REQUIRE(field_value(r, "type") == "1");
}

TEST_CASE("d2cs net bridge override clears on guard destruction",
          "[integration][legacy_d2cs][net_bridge]") {
    RecordingLogger sink;
    {
        ilc::BridgeLoggerOverride guard(sink);
        REQUIRE(::pvpgn_v3_d2cs_net_socket(1) == 0);
    }
    const std::size_t before = sink.records.size();
    REQUIRE(::pvpgn_v3_d2cs_net_check_connected(7) == 0);
    REQUIRE(sink.records.size() == before);
}
