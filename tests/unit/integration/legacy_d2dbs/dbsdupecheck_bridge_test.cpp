// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for the d2dbs dbsdupecheck observation bridge (R232).

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>

#include "core/logging.hpp"
#include "integration/legacy_d2dbs/bridge_logger.hpp"
#include "integration/legacy_d2dbs/dbsdupecheck_bridge.hpp"

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

TEST_CASE("dupecheck bridge logs datalen with non-null data",
          "[integration][legacy_d2dbs][dupecheck_bridge]") {
    RecordingLogger sink;
    ild::BridgeLoggerOverride guard(sink);
    char buf[16] = {0};
    REQUIRE(::pvpgn_v3_d2dbs_dupecheck_try(buf, 12u) == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.module  == "v3_d2dbs_dupecheck_bridge");
    REQUIRE(r.message == "dupecheck call observed");
    REQUIRE(field_value(r, "datalen") == "12");
    REQUIRE(field_value(r, "data")    == "present");
}

TEST_CASE("dupecheck bridge tags null data buffers",
          "[integration][legacy_d2dbs][dupecheck_bridge]") {
    RecordingLogger sink;
    ild::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2dbs_dupecheck_try(nullptr, 0u) == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(field_value(sink.records[0], "datalen") == "0");
    REQUIRE(field_value(sink.records[0], "data")    == "null");
}

TEST_CASE("dupecheck bridge returns 0 always (legacy fallthrough)",
          "[integration][legacy_d2dbs][dupecheck_bridge]") {
    RecordingLogger sink;
    ild::BridgeLoggerOverride guard(sink);
    char buf[1] = {0};
    REQUIRE(::pvpgn_v3_d2dbs_dupecheck_try(buf, 1u) == 0);
    REQUIRE(::pvpgn_v3_d2dbs_dupecheck_try(buf, 4096u) == 0);
}
