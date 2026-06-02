// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for the d2dbs server lifecycle observation bridges (R232).

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>

#include "core/logging.hpp"
#include "app/d2dbs/legacy_d2dbs_bridges/bridge_logger.hpp"
#include "app/d2dbs/legacy_d2dbs_bridges/dbserver_bridge.hpp"

namespace ild = pvpgn::integration::legacy_d2dbs;

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

TEST_CASE("dbserver main bridge logs",
          "[integration][legacy_d2dbs][dbserver_bridge]") {
    RecordingLogger sink;
    ild::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2dbs_server_main() == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.module  == "v3_d2dbs_server_bridge");
    REQUIRE(r.message == "server main entry observed");
    REQUIRE(r.fields.empty());
}

TEST_CASE("dbserver shutdown_connection bridge logs conn scalars",
          "[integration][legacy_d2dbs][dbserver_bridge]") {
    RecordingLogger sink;
    ild::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2dbs_server_shutdown_connection(
        7, 42u, 3u, 1u) == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.module  == "v3_d2dbs_server_bridge");
    REQUIRE(r.message == "server shutdown_connection observed");
    REQUIRE(field_value(r, "sd")       == "7");
    REQUIRE(field_value(r, "serverid") == "42");
    REQUIRE(field_value(r, "type")     == "3");
    REQUIRE(field_value(r, "verified") == "1");
}

TEST_CASE("dbserver shutdown_connection bridge handles negative sd",
          "[integration][legacy_d2dbs][dbserver_bridge]") {
    RecordingLogger sink;
    ild::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2dbs_server_shutdown_connection(
        -1, 0u, 0u, 0u) == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(field_value(sink.records[0], "sd")       == "-1");
    REQUIRE(field_value(sink.records[0], "verified") == "0");
}
