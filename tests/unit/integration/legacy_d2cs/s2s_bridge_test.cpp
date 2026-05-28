// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for the d2cs s2s observation bridge (R233).

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>

#include "core/logging.hpp"
#include "integration/legacy_d2cs/bridge_logger.hpp"
#include "integration/legacy_d2cs/s2s_bridge.hpp"

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

}  // namespace

TEST_CASE("d2cs s2s init bridge logs at info",
          "[integration][legacy_d2cs][s2s_bridge]") {
    RecordingLogger sink;
    ilc::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_s2s_init_try() == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Info);
    REQUIRE(r.module  == "v3_d2cs_s2s_bridge");
    REQUIRE(r.message == "s2s init observed");
    REQUIRE(r.fields.empty());
}

TEST_CASE("d2cs s2s bridge override clears on guard destruction",
          "[integration][legacy_d2cs][s2s_bridge]") {
    RecordingLogger sink;
    {
        ilc::BridgeLoggerOverride guard(sink);
        REQUIRE(::pvpgn_v3_d2cs_s2s_init_try() == 0);
    }
    const std::size_t before = sink.records.size();
    REQUIRE(::pvpgn_v3_d2cs_s2s_init_try() == 0);
    REQUIRE(sink.records.size() == before);
}
