// SPDX-License-Identifier: GPL-2.0-or-later
// R246: second batch of bnetd small-module observation bridges
// (i18n, icons, attrlayer, tracker, team, udptest_send).

#include "integration/legacy_bnetd/bnetd_lifecycle_bridges_r246.hpp"

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

}  // namespace

// ---- i18n ---------------------------------------------------------

extern "C" int pvpgn_v3_bnetd_i18n_load_try(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_i18n_bridge",
        "i18n load observed", {});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_i18n_reload_try(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_i18n_bridge",
        "i18n reload observed", {});
    return 0;
}

// ---- icons --------------------------------------------------------

extern "C" int pvpgn_v3_bnetd_icons_load_try(
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

extern "C" int pvpgn_v3_bnetd_icons_unload_try(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_bnetd_icons_bridge",
        "customicons unload observed", {});
    return 0;
}

// ---- attrlayer ----------------------------------------------------

extern "C" int pvpgn_v3_bnetd_attrlayer_init_try(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_bnetd_attrlayer_bridge",
        "attrlayer init observed", {});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_attrlayer_cleanup_try(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_bnetd_attrlayer_bridge",
        "attrlayer cleanup observed", {});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_attrlayer_save_try(int flags) noexcept {
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

extern "C" int pvpgn_v3_bnetd_attrlayer_flush_try(int flags) noexcept {
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

// ---- tracker ------------------------------------------------------

extern "C" int pvpgn_v3_bnetd_tracker_set_servers_try(
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

extern "C" int pvpgn_v3_bnetd_tracker_send_report_try(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Trace,
        "v3_bnetd_tracker_bridge",
        "tracker send_report observed", {});
    return 0;
}

// ---- team ---------------------------------------------------------

extern "C" int pvpgn_v3_bnetd_team_load_try(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_bnetd_team_bridge",
        "teamlist load observed", {});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_team_unload_try(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_bnetd_team_bridge",
        "teamlist unload observed", {});
    return 0;
}

// ---- udptest ------------------------------------------------------

extern "C" int pvpgn_v3_bnetd_udptest_send_try(int sd) noexcept {
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
