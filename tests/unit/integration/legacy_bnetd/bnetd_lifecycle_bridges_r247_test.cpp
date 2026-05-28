// SPDX-License-Identifier: GPL-2.0-or-later
// R247 -- unit tests for the bnetd third batch of small-module
// lifecycle bridges (alias_command, command_groups,
// anongame_maplists, handle_udp).

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/bnetd_lifecycle_bridges_r247.hpp"
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

// ---- alias_command ------------------------------------------------

TEST_CASE("aliasfile load bridge logs filename at Info",
          "[integration][legacy_bnetd][alias_command_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_aliasfile_load_try("/etc/pvpgn/bnalias.conf") == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level == pvpgn::core::LogLevel::Info);
    REQUIRE(std::string{r.module} == "v3_bnetd_alias_command_bridge");
    REQUIRE(std::string{r.message} == "aliasfile load observed");
    REQUIRE(std::string{field_value(r, "filename")}
            == "/etc/pvpgn/bnalias.conf");
}

TEST_CASE("aliasfile load tolerates null filename",
          "[integration][legacy_bnetd][alias_command_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_aliasfile_load_try(nullptr) == 0);
    REQUIRE(std::string{field_value(sink.records.front(), "filename")}
            == "<null>");
}

TEST_CASE("aliasfile unload bridge logs at Info",
          "[integration][legacy_bnetd][alias_command_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_aliasfile_unload_try() == 0);
    REQUIRE(std::string{sink.records.front().message}
            == "aliasfile unload observed");
}

TEST_CASE("handle_alias_command bridge logs sd and text at Debug",
          "[integration][legacy_bnetd][alias_command_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_handle_alias_command_try(11, "/away brb") == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level == pvpgn::core::LogLevel::Debug);
    REQUIRE(std::string{r.message} == "handle_alias_command observed");
    REQUIRE(std::string{field_value(r, "sd")}   == "11");
    REQUIRE(std::string{field_value(r, "text")} == "/away brb");
}

TEST_CASE("handle_alias_command encodes null sentinel and null text",
          "[integration][legacy_bnetd][alias_command_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_handle_alias_command_try(-1, nullptr) == 0);
    REQUIRE(std::string{field_value(sink.records.front(), "sd")}   == "-1");
    REQUIRE(std::string{field_value(sink.records.front(), "text")} == "<null>");
}

// ---- command_groups ----------------------------------------------

TEST_CASE("command_groups load bridge logs filename at Info",
          "[integration][legacy_bnetd][command_groups_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_command_groups_load_try(
                "/etc/pvpgn/command_groups.conf") == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level == pvpgn::core::LogLevel::Info);
    REQUIRE(std::string{r.module} == "v3_bnetd_command_groups_bridge");
    REQUIRE(std::string{r.message} == "command_groups load observed");
    REQUIRE(std::string{field_value(r, "filename")}
            == "/etc/pvpgn/command_groups.conf");
}

TEST_CASE("command_groups unload bridge logs at Info",
          "[integration][legacy_bnetd][command_groups_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_command_groups_unload_try() == 0);
    REQUIRE(std::string{sink.records.front().message}
            == "command_groups unload observed");
}

TEST_CASE("command_groups reload bridge logs filename at Info",
          "[integration][legacy_bnetd][command_groups_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_command_groups_reload_try("cg.conf") == 0);
    REQUIRE(std::string{sink.records.front().message}
            == "command_groups reload observed");
    REQUIRE(std::string{field_value(sink.records.front(), "filename")}
            == "cg.conf");
}

// ---- anongame_maplists -------------------------------------------

TEST_CASE("anongame_maplists create bridge logs at Info",
          "[integration][legacy_bnetd][anongame_maplists_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_anongame_maplists_create_try() == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level == pvpgn::core::LogLevel::Info);
    REQUIRE(std::string{r.module}
            == "v3_bnetd_anongame_maplists_bridge");
    REQUIRE(std::string{r.message}
            == "anongame_maplists create observed");
}

TEST_CASE("anongame_maplists destroy bridge logs at Info",
          "[integration][legacy_bnetd][anongame_maplists_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_anongame_maplists_destroy_try() == 0);
    REQUIRE(std::string{sink.records.front().message}
            == "anongame_maplists destroy observed");
}

TEST_CASE("anongame_tournament_maplists destroy bridge logs at Info",
          "[integration][legacy_bnetd][anongame_maplists_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_anongame_tournament_maplists_destroy_try() == 0);
    REQUIRE(std::string{sink.records.front().message}
            == "anongame_tournament_maplists destroy observed");
}

// ---- handle_udp --------------------------------------------------

TEST_CASE("handle_udp_packet bridge logs all fields at Trace",
          "[integration][legacy_bnetd][handle_udp_bridge]") {
    RecordingLogger sink;
    ila::BridgeLoggerOverride guard(sink);
    REQUIRE(::pvpgn_v3_bnetd_handle_udp_packet_try(
                42, 0xC0A80101u, 6112u) == 0);
    REQUIRE(sink.records.size() == 1u);
    const auto& r = sink.records.front();
    REQUIRE(r.level == pvpgn::core::LogLevel::Trace);
    REQUIRE(std::string{r.module} == "v3_bnetd_handle_udp_bridge");
    REQUIRE(std::string{r.message} == "handle_udp_packet observed");
    REQUIRE(std::string{field_value(r, "usock")}    == "42");
    REQUIRE(std::string{field_value(r, "src_addr")} == "3232235777");
    REQUIRE(std::string{field_value(r, "src_port")} == "6112");
}
