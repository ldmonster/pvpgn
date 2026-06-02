// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for the d2cs d2gs-list lifecycle observation bridge (R236).

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>

#include "core/logging.hpp"
#include "app/d2cs/legacy_d2cs_bridges/bridge_logger.hpp"
#include "app/d2cs/legacy_d2cs_bridges/d2gs_bridge.hpp"

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

TEST_CASE("d2cs d2gslist create bridge logs at debug",
          "[integration][legacy_d2cs][d2gs_bridge]") {
    RecordingLogger sink;
    ilc::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_d2gslist_create() == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Debug);
    REQUIRE(r.module  == "v3_d2cs_d2gs_bridge");
    REQUIRE(r.message == "d2gslist create observed");
    REQUIRE(r.fields.empty());
}

TEST_CASE("d2cs d2gslist destroy bridge logs at debug",
          "[integration][legacy_d2cs][d2gs_bridge]") {
    RecordingLogger sink;
    ilc::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_d2gslist_destroy() == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.module  == "v3_d2cs_d2gs_bridge");
    REQUIRE(r.message == "d2gslist destroy observed");
}

TEST_CASE("d2cs d2gslist reload bridge logs gslist string",
          "[integration][legacy_d2cs][d2gs_bridge]") {
    RecordingLogger sink;
    ilc::BridgeLoggerOverride guard(sink);
    const char* gslist = "10.0.0.1:6113,10.0.0.2:6113";
    REQUIRE(::pvpgn_v3_d2cs_d2gslist_reload(gslist) == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.message == "d2gslist reload observed");
    REQUIRE(r.fields.size() == 1u);
    REQUIRE(field_value(r, "gslist") == "10.0.0.1:6113,10.0.0.2:6113");
}

TEST_CASE("d2cs d2gslist reload bridge handles null gslist",
          "[integration][legacy_d2cs][d2gs_bridge]") {
    RecordingLogger sink;
    ilc::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_d2cs_d2gslist_reload(nullptr) == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(field_value(sink.records.front(), "gslist") == "<null>");
}

TEST_CASE("d2cs d2gslist bridge override clears on guard destruction",
          "[integration][legacy_d2cs][d2gs_bridge]") {
    RecordingLogger sink;
    {
        ilc::BridgeLoggerOverride guard(sink);
        REQUIRE(::pvpgn_v3_d2cs_d2gslist_create() == 0);
    }
    const std::size_t before = sink.records.size();
    REQUIRE(::pvpgn_v3_d2cs_d2gslist_destroy() == 0);
    REQUIRE(sink.records.size() == before);
}
