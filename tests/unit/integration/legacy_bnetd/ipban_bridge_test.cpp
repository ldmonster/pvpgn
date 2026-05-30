// SPDX-License-Identifier: GPL-2.0-or-later
// R244 -- unit tests for bnetd IP-ban observation bridges.

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"
#include "integration/legacy_bnetd/ipban_bridge.hpp"

namespace ila = pvpgn::integration::legacy_bnetd;

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

TEST_CASE("ipban create bridge logs Debug",
          "[integration][legacy_bnetd][ipban_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_ipban_create() == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Debug);
    REQUIRE(r.module  == "v3_bnetd_ipban_bridge");
    REQUIRE(r.message == "ipbanlist create observed");
}

TEST_CASE("ipban destroy bridge logs Debug",
          "[integration][legacy_bnetd][ipban_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_ipban_destroy() == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(sink.records.front().message == "ipbanlist destroy observed");
}

TEST_CASE("ipban load bridge records filename",
          "[integration][legacy_bnetd][ipban_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_ipban_load("/etc/pvpgn/bnban") == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level == pvpgn::core::LogLevel::Info);
    REQUIRE(std::string{field_value(r, "filename")} == "/etc/pvpgn/bnban");
}

TEST_CASE("ipban load bridge tolerates null filename",
          "[integration][legacy_bnetd][ipban_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_ipban_load(nullptr) == 0);
    REQUIRE(std::string{field_value(sink.records.front(), "filename")}
            == "<null>");
}

TEST_CASE("ipban save bridge records filename",
          "[integration][legacy_bnetd][ipban_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_ipban_save("/var/lib/pvpgn/bnban") == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Info);
    REQUIRE(r.message == "ipbanlist save observed");
    REQUIRE(std::string{field_value(r, "filename")}
            == "/var/lib/pvpgn/bnban");
}

TEST_CASE("ipban check bridge records ipaddr at Trace",
          "[integration][legacy_bnetd][ipban_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_ipban_check("192.0.2.7") == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Trace);
    REQUIRE(r.message == "ipbanlist check observed");
    REQUIRE(std::string{field_value(r, "ipaddr")} == "192.0.2.7");
}

TEST_CASE("ipban check bridge tolerates null ipaddr",
          "[integration][legacy_bnetd][ipban_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_ipban_check(nullptr) == 0);
    REQUIRE(std::string{field_value(sink.records.front(), "ipaddr")}
            == "<null>");
}

TEST_CASE("ipban add bridge records sd / ipaddr / endtime",
          "[integration][legacy_bnetd][ipban_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_ipban_add(
        9, "203.0.113.0/24", 1700000123ULL) == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Info);
    REQUIRE(r.message == "ipbanlist add observed");
    REQUIRE(std::string{field_value(r, "sd")}      == "9");
    REQUIRE(std::string{field_value(r, "ipaddr")}  == "203.0.113.0/24");
    REQUIRE(std::string{field_value(r, "endtime")} == "1700000123");
}

TEST_CASE("ipban add bridge handles no-admin context (sd=-1, permanent)",
          "[integration][legacy_bnetd][ipban_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_ipban_add(-1, nullptr, 0ULL) == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(std::string{field_value(r, "sd")}      == "-1");
    REQUIRE(std::string{field_value(r, "ipaddr")}  == "<null>");
    REQUIRE(std::string{field_value(r, "endtime")} == "0");
}

TEST_CASE("ipban unload_expired bridge logs Debug",
          "[integration][legacy_bnetd][ipban_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_ipban_unload_expired() == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Debug);
    REQUIRE(r.message == "ipbanlist unload_expired observed");
}
