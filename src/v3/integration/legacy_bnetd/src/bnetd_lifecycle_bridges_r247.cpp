// SPDX-License-Identifier: GPL-2.0-or-later
// R247: third batch of bnetd small-module observation bridges
// (alias_command, command_groups, anongame_maplists, handle_udp).

#include "integration/legacy_bnetd/bnetd_lifecycle_bridges_r247.hpp"

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

// ---- alias_command ------------------------------------------------

extern "C" int pvpgn_v3_bnetd_aliasfile_load_try(
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

extern "C" int pvpgn_v3_bnetd_aliasfile_unload_try(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_alias_command_bridge",
        "aliasfile unload observed", {});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_handle_alias_command_try(
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

// ---- command_groups ----------------------------------------------

extern "C" int pvpgn_v3_bnetd_command_groups_load_try(
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

extern "C" int pvpgn_v3_bnetd_command_groups_unload_try(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_command_groups_bridge",
        "command_groups unload observed", {});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_command_groups_reload_try(
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

// ---- anongame_maplists -------------------------------------------

extern "C" int pvpgn_v3_bnetd_anongame_maplists_create_try(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_anongame_maplists_bridge",
        "anongame_maplists create observed", {});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_anongame_maplists_destroy_try(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_anongame_maplists_bridge",
        "anongame_maplists destroy observed", {});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_anongame_tournament_maplists_destroy_try(
    void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_anongame_maplists_bridge",
        "anongame_tournament_maplists destroy observed", {});
    return 0;
}

// ---- handle_udp --------------------------------------------------

extern "C" int pvpgn_v3_bnetd_handle_udp_packet_try(
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
