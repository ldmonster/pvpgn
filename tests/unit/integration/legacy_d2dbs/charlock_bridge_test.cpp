// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for the d2dbs charlock observation bridge (R231).

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>

#include "core/logging.hpp"
#include "integration/legacy_d2dbs/bridge_logger.hpp"
#include "integration/legacy_d2dbs/charlock_bridge.hpp"

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

TEST_CASE("charlock init bridge logs sizing",
          "[integration][legacy_d2dbs][charlock_bridge]") {
    RecordingLogger sink;
    ild::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2dbs_charlock_init_try(65000u, 32u) == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.module  == "v3_d2dbs_charlock_bridge");
    REQUIRE(r.message == "charlock init observed");
    REQUIRE(field_value(r, "tbllen") == "65000");
    REQUIRE(field_value(r, "maxgs")  == "32");
}

TEST_CASE("charlock init bridge zero arguments",
          "[integration][legacy_d2dbs][charlock_bridge]") {
    RecordingLogger sink;
    ild::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2dbs_charlock_init_try(0u, 0u) == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(field_value(sink.records[0], "tbllen") == "0");
    REQUIRE(field_value(sink.records[0], "maxgs")  == "0");
}

TEST_CASE("charlock destroy bridge logs",
          "[integration][legacy_d2dbs][charlock_bridge]") {
    RecordingLogger sink;
    ild::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2dbs_charlock_destroy_try() == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.module  == "v3_d2dbs_charlock_bridge");
    REQUIRE(r.message == "charlock destroy observed");
    REQUIRE(r.fields.empty());
}
