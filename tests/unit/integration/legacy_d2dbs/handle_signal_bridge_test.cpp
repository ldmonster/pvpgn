// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for the d2dbs signal lifecycle observation bridges.

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>

#include "core/logging.hpp"
#include "app/d2dbs/legacy_d2dbs_bridges/bridge_logger.hpp"
#include "app/d2dbs/legacy_d2dbs_bridges/handle_signal_bridge.hpp"

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

TEST_CASE("signal init bridge logs",
          "[integration][legacy_d2dbs][signal_bridge]") {
    RecordingLogger sink;
    ild::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2dbs_handle_signal_init() == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.module  == "v3_d2dbs_signal_bridge");
    REQUIRE(r.message == "signal init observed");
    REQUIRE(r.fields.empty());
}

TEST_CASE("signal dispatch bridge logs",
          "[integration][legacy_d2dbs][signal_bridge]") {
    RecordingLogger sink;
    ild::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2dbs_handle_signal() == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.module  == "v3_d2dbs_signal_bridge");
    REQUIRE(r.message == "signal dispatch observed");
    REQUIRE(r.fields.empty());
}

TEST_CASE("signal bridges return 0 (legacy fallthrough)",
          "[integration][legacy_d2dbs][signal_bridge]") {
    RecordingLogger sink;
    ild::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2dbs_handle_signal_init() == 0);
    REQUIRE(::pvpgn_v3_d2dbs_handle_signal() == 0);
    REQUIRE(::pvpgn_v3_d2dbs_handle_signal() == 0);
    REQUIRE(sink.records.size() == 3u);
}
