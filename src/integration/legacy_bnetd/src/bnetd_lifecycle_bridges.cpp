// SPDX-License-Identifier: GPL-2.0-or-later
// Unified bnetd small-module lifecycle observation bridges.
// Merged from three revision batches:
//   R245: helpfile, autoupdate, output, support, mail
//   R246: i18n, icons, attrlayer, tracker, team, udptest_send
//   R247: alias_command, command_groups, anongame_maplists, handle_udp
//
// All bridges are observation-only (return 0; legacy MUST fall through).
// The _r246 and _r247 source files have been collapsed into this file
// because r247 is the only live revision path in 3.0.0.
// See: plans/16-strangler-completion-detail.md §5 "Can be done now"

#include "integration/legacy_bnetd/bnetd_lifecycle_bridges.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <string_view>
#include <system_error>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"

namespace plb = pvpgn::integration::legacy_bnetd;

namespace {

constexpr std::string_view kNull = "<null>";

std::string_view safe_str(const char* s) noexcept {
    if (!s) return kNull;
    return std::string_view{s};
}

std::string_view render_int(std::array<char, 24>& buf, int v) noexcept {
    auto res = std::to_chars(buf.data(), buf.data() + buf.size(), v);
    if (res.ec != std::errc{}) return std::string_view{};
    return std::string_view{buf.data(),
        static_cast<std::size_t>(res.ptr - buf.data())};
}

std::string_view render_uint(std::array<char, 24>& buf,
                              unsigned int v) noexcept {
    auto res = std::to_chars(buf.data(), buf.data() + buf.size(), v);
    if (res.ec != std::errc{}) return std::string_view{};
    return std::string_view{buf.data(),
        static_cast<std::size_t>(res.ptr - buf.data())};
}

}  // namespace

// ===========================================================================
// R245: helpfile
// ===========================================================================

extern "C" int pvpgn_v3_bnetd_helpfile_init(
    const char* filename) noexcept {
    const pvpgn::core::ILogger::Field fields[] = {
        {"filename", safe_str(filename)},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_helpfile_bridge",
        "helpfile init observed",
        {fields[0]});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_helpfile_unload(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_helpfile_bridge",
        "helpfile unload observed",
        {});
    return 0;
}

// ===========================================================================
// R245: autoupdate
// ===========================================================================

extern "C" int pvpgn_v3_bnetd_autoupdate_load(
    const char* filename) noexcept {
    const pvpgn::core::ILogger::Field fields[] = {
        {"filename", safe_str(filename)},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_autoupdate_bridge",
        "autoupdate load observed",
        {fields[0]});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_autoupdate_unload(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_autoupdate_bridge",
        "autoupdate unload observed",
        {});
    return 0;
}

// ===========================================================================
// R245: output
// ===========================================================================

extern "C" int pvpgn_v3_bnetd_output_init(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_bnetd_output_bridge",
        "output init observed",
        {});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_output_write_to_file(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_bnetd_output_bridge",
        "output write_to_file observed",
        {});
    return 0;
}

// ===========================================================================
// R245: support
// ===========================================================================

extern "C" int pvpgn_v3_bnetd_support_check_files(
    const char* supportfile) noexcept {
    const pvpgn::core::ILogger::Field fields[] = {
        {"supportfile", safe_str(supportfile)},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_support_bridge",
        "support check_files observed",
        {fields[0]});
    return 0;
}

// ===========================================================================
// R245: mail
// ===========================================================================

extern "C" int pvpgn_v3_bnetd_mail_handle_command(
    int sd,
    const char* text) noexcept {
    std::array<char, 24> sdbuf{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"sd",   render_int(sdbuf, sd)},
        {"text", safe_str(text)},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_bnetd_mail_bridge",
        "mail handle_command observed",
        {fields[0], fields[1]});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_mail_check(int sd) noexcept {
    std::array<char, 24> sdbuf{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"sd", render_int(sdbuf, sd)},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Trace,
        "v3_bnetd_mail_bridge",
        "mail check observed",
        {fields[0]});
    return 0;
}

// ===========================================================================
// R246: i18n
// ===========================================================================

extern "C" int pvpgn_v3_bnetd_i18n_load(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_i18n_bridge",
        "i18n load observed", {});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_i18n_reload(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_i18n_bridge",
        "i18n reload observed", {});
    return 0;
}

// ===========================================================================
// R246: icons
// ===========================================================================

extern "C" int pvpgn_v3_bnetd_icons_load(
    const char* filename) noexcept {
    const pvpgn::core::ILogger::Field fields[] = {
        {"filename", safe_str(filename)},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_icons_bridge",
        "customicons load observed",
        {fields[0]});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_icons_unload(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_icons_bridge",
        "customicons unload observed", {});
    return 0;
}

// ===========================================================================
// R246: attrlayer
// ===========================================================================

extern "C" int pvpgn_v3_bnetd_attrlayer_init(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_bnetd_attrlayer_bridge",
        "attrlayer init observed", {});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_attrlayer_cleanup(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_bnetd_attrlayer_bridge",
        "attrlayer cleanup observed", {});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_attrlayer_save(int flags) noexcept {
    std::array<char, 24> buf{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"flags", render_int(buf, flags)},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_attrlayer_bridge",
        "attrlayer save observed",
        {fields[0]});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_attrlayer_flush(int flags) noexcept {
    std::array<char, 24> buf{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"flags", render_int(buf, flags)},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_attrlayer_bridge",
        "attrlayer flush observed",
        {fields[0]});
    return 0;
}

// ===========================================================================
// R246: tracker
// ===========================================================================

extern "C" int pvpgn_v3_bnetd_tracker_set_servers(
    const char* servers) noexcept {
    const pvpgn::core::ILogger::Field fields[] = {
        {"servers", safe_str(servers)},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_tracker_bridge",
        "tracker set_servers observed",
        {fields[0]});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_tracker_send_report(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Trace,
        "v3_bnetd_tracker_bridge",
        "tracker send_report observed", {});
    return 0;
}

// ===========================================================================
// R246: team
// ===========================================================================

extern "C" int pvpgn_v3_bnetd_team_load(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_bnetd_team_bridge",
        "teamlist load observed", {});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_team_unload(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_bnetd_team_bridge",
        "teamlist unload observed", {});
    return 0;
}

// ===========================================================================
// R246: udptest
// ===========================================================================

extern "C" int pvpgn_v3_bnetd_udptest_send(int sd) noexcept {
    std::array<char, 24> buf{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"sd", render_int(buf, sd)},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Trace,
        "v3_bnetd_udptest_bridge",
        "udptest send observed",
        {fields[0]});
    return 0;
}

// ===========================================================================
// R247: alias_command
// ===========================================================================

extern "C" int pvpgn_v3_bnetd_aliasfile_load(
    const char* filename) noexcept {
    const pvpgn::core::ILogger::Field fields[] = {
        {"filename", safe_str(filename)},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_alias_command_bridge",
        "aliasfile load observed",
        {fields[0]});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_aliasfile_unload(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_alias_command_bridge",
        "aliasfile unload observed", {});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_handle_alias_command(
    int sd, const char* text) noexcept {
    std::array<char, 24> buf{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"sd",   render_int(buf, sd)},
        {"text", safe_str(text)},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_bnetd_alias_command_bridge",
        "handle_alias_command observed",
        {fields[0], fields[1]});
    return 0;
}

// ===========================================================================
// R247: command_groups
// ===========================================================================

extern "C" int pvpgn_v3_bnetd_command_groups_load(
    const char* filename) noexcept {
    const pvpgn::core::ILogger::Field fields[] = {
        {"filename", safe_str(filename)},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_command_groups_bridge",
        "command_groups load observed",
        {fields[0]});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_command_groups_unload(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_command_groups_bridge",
        "command_groups unload observed", {});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_command_groups_reload(
    const char* filename) noexcept {
    const pvpgn::core::ILogger::Field fields[] = {
        {"filename", safe_str(filename)},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_command_groups_bridge",
        "command_groups reload observed",
        {fields[0]});
    return 0;
}

// ===========================================================================
// R247: anongame_maplists
// ===========================================================================

extern "C" int pvpgn_v3_bnetd_anongame_maplists_create(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_anongame_maplists_bridge",
        "anongame_maplists create observed", {});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_anongame_maplists_destroy(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_anongame_maplists_bridge",
        "anongame_maplists destroy observed", {});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_anongame_tournament_maplists_destroy(
    void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_anongame_maplists_bridge",
        "anongame_tournament_maplists destroy observed", {});
    return 0;
}

// ===========================================================================
// R247: handle_udp
// ===========================================================================

extern "C" int pvpgn_v3_bnetd_handle_udp_packet(
    int usock, unsigned int src_addr,
    unsigned int src_port) noexcept {
    std::array<char, 24> b1{};
    std::array<char, 24> b2{};
    std::array<char, 24> b3{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"usock",    render_int(b1, usock)},
        {"src_addr", render_uint(b2, src_addr)},
        {"src_port", render_uint(b3, src_port)},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Trace,
        "v3_bnetd_handle_udp_bridge",
        "handle_udp_packet observed",
        {fields[0], fields[1], fields[2]});
    return 0;
}
