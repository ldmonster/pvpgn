// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for the d2dbs dbspacket dispatcher observation bridges (R242).

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>

#include "core/logging.hpp"
#include "integration/legacy_d2dbs/bridge_logger.hpp"
#include "integration/legacy_d2dbs/dbspacket_bridge.hpp"

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
    return std::string{};
}

}  // namespace

TEST_CASE("d2dbs packet_handle bridge logs sd/stats/type at trace",
          "[integration][legacy_d2dbs][dbspacket_bridge]") {
    RecordingLogger sink;
    ild::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2dbs_packet_handle_try(11, 1u, 7u) == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Trace);
    REQUIRE(r.module  == "v3_d2dbs_dbspacket_bridge");
    REQUIRE(r.message == "packet handle observed");
    REQUIRE(r.fields.size() == 3u);
    REQUIRE(field_value(r, "sd")    == "11");
    REQUIRE(field_value(r, "stats") == "1");
    REQUIRE(field_value(r, "type")  == "7");
}

TEST_CASE("d2dbs check_timeout bridge logs at debug",
          "[integration][legacy_d2dbs][dbspacket_bridge]") {
    RecordingLogger sink;
    ild::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2dbs_check_timeout_try() == 0);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Debug);
    REQUIRE(r.message == "check_timeout observed");
    REQUIRE(r.fields.empty());
}

TEST_CASE("d2dbs keepalive bridge logs at debug",
          "[integration][legacy_d2dbs][dbspacket_bridge]") {
    RecordingLogger sink;
    ild::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2dbs_keepalive_try() == 0);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Debug);
    REQUIRE(r.message == "keepalive observed");
    REQUIRE(r.fields.empty());
}

TEST_CASE("d2dbs dbspacket bridge override clears on guard destruction",
          "[integration][legacy_d2dbs][dbspacket_bridge]") {
    RecordingLogger sink;
    {
        ild::BridgeLoggerOverride guard(sink);
        REQUIRE(::pvpgn_v3_d2dbs_keepalive_try() == 0);
    }
    const std::size_t before = sink.records.size();
    REQUIRE(::pvpgn_v3_d2dbs_check_timeout_try() == 0);
    REQUIRE(sink.records.size() == before);
}
