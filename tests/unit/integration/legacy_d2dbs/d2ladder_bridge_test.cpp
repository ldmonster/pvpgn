// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for the d2dbs d2ladder observation bridge.

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>

#include "core/logging.hpp"
#include "app/d2dbs/legacy_d2dbs_bridges/bridge_logger.hpp"
#include "app/d2dbs/legacy_d2dbs_bridges/d2ladder_bridge.hpp"

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

}  // namespace

TEST_CASE("d2ladder init bridge logs",
          "[integration][legacy_d2dbs][d2ladder_bridge]") {
    RecordingLogger sink;
    ild::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2dbs_d2ladder_init() == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.module  == "v3_d2dbs_d2ladder_bridge");
    REQUIRE(r.message == "d2ladder init observed");
    REQUIRE(r.fields.empty());
}

TEST_CASE("d2ladder destroy bridge logs",
          "[integration][legacy_d2dbs][d2ladder_bridge]") {
    RecordingLogger sink;
    ild::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2dbs_d2ladder_destroy() == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.module  == "v3_d2dbs_d2ladder_bridge");
    REQUIRE(r.message == "d2ladder destroy observed");
    REQUIRE(r.fields.empty());
}

TEST_CASE("d2ladder bridge override clears on guard destruction",
          "[integration][legacy_d2dbs][d2ladder_bridge]") {
    RecordingLogger sink;
    {
        ild::BridgeLoggerOverride guard(sink);
        REQUIRE(::pvpgn_v3_d2dbs_d2ladder_init() == 0);
    }
    // After guard goes out of scope, the override is cleared and the
    // sink must NOT receive any further records.
    const std::size_t before = sink.records.size();
    REQUIRE(::pvpgn_v3_d2dbs_d2ladder_destroy() == 0);
    REQUIRE(sink.records.size() == before);
}
