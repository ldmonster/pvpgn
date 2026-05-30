// SPDX-License-Identifier: GPL-2.0-or-later
// R246 -- unit tests for the bnetd second batch of small-module
// lifecycle bridges (i18n, icons, attrlayer, tracker, team,
// udptest_send).

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/bnetd_lifecycle_bridges_r246.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"

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

// ---- i18n ---------------------------------------------------------

TEST_CASE("i18n load bridge logs at Info",
          "[integration][legacy_bnetd][i18n_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_i18n_load() == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level   == pvpgn::core::LogLevel::Info);
    REQUIRE(std::string{r.module}  == "v3_bnetd_i18n_bridge");
    REQUIRE(std::string{r.message} == "i18n load observed");
}

TEST_CASE("i18n reload bridge logs at Info",
          "[integration][legacy_bnetd][i18n_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_i18n_reload() == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(std::string{sink.records.front().message}
            == "i18n reload observed");
}

// ---- icons --------------------------------------------------------

TEST_CASE("icons load bridge logs filename at Info",
          "[integration][legacy_bnetd][icons_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_icons_load("/etc/pvpgn/bnicons.conf") == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level == pvpgn::core::LogLevel::Info);
    REQUIRE(std::string{r.module} == "v3_bnetd_icons_bridge");
    REQUIRE(std::string{r.message} == "customicons load observed");
    REQUIRE(std::string{field_value(r, "filename")}
            == "/etc/pvpgn/bnicons.conf");
}

TEST_CASE("icons load tolerates null filename",
          "[integration][legacy_bnetd][icons_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_icons_load(nullptr) == 0);
    REQUIRE(std::string{field_value(sink.records.front(), "filename")}
            == "<null>");
}

TEST_CASE("icons unload bridge logs at Info",
          "[integration][legacy_bnetd][icons_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_icons_unload() == 0);
    REQUIRE(sink.records.size() == 1u);
    REQUIRE(std::string{sink.records.front().message}
            == "customicons unload observed");
}

// ---- attrlayer ----------------------------------------------------

TEST_CASE("attrlayer init bridge logs at Debug",
          "[integration][legacy_bnetd][attrlayer_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_attrlayer_init() == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level == pvpgn::core::LogLevel::Debug);
    REQUIRE(std::string{r.module} == "v3_bnetd_attrlayer_bridge");
    REQUIRE(std::string{r.message} == "attrlayer init observed");
}

TEST_CASE("attrlayer cleanup bridge logs at Debug",
          "[integration][legacy_bnetd][attrlayer_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_attrlayer_cleanup() == 0);
    REQUIRE(std::string{sink.records.front().message}
            == "attrlayer cleanup observed");
}

TEST_CASE("attrlayer save bridge logs flags at Info",
          "[integration][legacy_bnetd][attrlayer_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_attrlayer_save(7) == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level == pvpgn::core::LogLevel::Info);
    REQUIRE(std::string{r.message} == "attrlayer save observed");
    REQUIRE(std::string{field_value(r, "flags")} == "7");
}

TEST_CASE("attrlayer flush bridge logs flags at Info",
          "[integration][legacy_bnetd][attrlayer_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_attrlayer_flush(3) == 0);
    REQUIRE(std::string{field_value(sink.records.front(), "flags")}
            == "3");
}

// ---- tracker ------------------------------------------------------

TEST_CASE("tracker set_servers bridge logs servers at Info",
          "[integration][legacy_bnetd][tracker_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_tracker_set_servers(
                "track.pvpgn.pro:6114") == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level == pvpgn::core::LogLevel::Info);
    REQUIRE(std::string{r.module} == "v3_bnetd_tracker_bridge");
    REQUIRE(std::string{r.message} == "tracker set_servers observed");
    REQUIRE(std::string{field_value(r, "servers")}
            == "track.pvpgn.pro:6114");
}

TEST_CASE("tracker set_servers tolerates null",
          "[integration][legacy_bnetd][tracker_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_tracker_set_servers(nullptr) == 0);
    REQUIRE(std::string{field_value(sink.records.front(), "servers")}
            == "<null>");
}

TEST_CASE("tracker send_report bridge logs at Trace",
          "[integration][legacy_bnetd][tracker_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_tracker_send_report() == 0);
    REQUIRE(sink.records.front().level
            == pvpgn::core::LogLevel::Trace);
    REQUIRE(std::string{sink.records.front().message}
            == "tracker send_report observed");
}

// ---- team ---------------------------------------------------------

TEST_CASE("team load bridge logs at Debug",
          "[integration][legacy_bnetd][team_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_team_load() == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level == pvpgn::core::LogLevel::Debug);
    REQUIRE(std::string{r.module} == "v3_bnetd_team_bridge");
    REQUIRE(std::string{r.message} == "teamlist load observed");
}

TEST_CASE("team unload bridge logs at Debug",
          "[integration][legacy_bnetd][team_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_team_unload() == 0);
    REQUIRE(std::string{sink.records.front().message}
            == "teamlist unload observed");
}

// ---- udptest ------------------------------------------------------

TEST_CASE("udptest send bridge logs sd at Trace",
          "[integration][legacy_bnetd][udptest_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_udptest_send(42) == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level == pvpgn::core::LogLevel::Trace);
    REQUIRE(std::string{r.module} == "v3_bnetd_udptest_bridge");
    REQUIRE(std::string{r.message} == "udptest send observed");
    REQUIRE(std::string{field_value(r, "sd")} == "42");
}

TEST_CASE("udptest send bridge encodes null-connection sentinel",
          "[integration][legacy_bnetd][udptest_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_udptest_send(-1) == 0);
    REQUIRE(std::string{field_value(sink.records.front(), "sd")}
            == "-1");
}
