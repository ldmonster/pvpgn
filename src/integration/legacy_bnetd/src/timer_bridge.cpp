// SPDX-License-Identifier: GPL-2.0-or-later
//
// R243: observation bridges for bnetd per-connection timer subsystem.

#include "integration/legacy_bnetd/timer_bridge.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <string_view>
#include <system_error>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"

namespace plb = pvpgn::integration::legacy_bnetd;

namespace {

std::string_view render_int(std::array<char, 24>& buf, int v) noexcept {
    auto res = std::to_chars(buf.data(), buf.data() + buf.size(), v);
    if (res.ec != std::errc{}) return std::string_view{};
    return std::string_view{buf.data(),
        static_cast<std::size_t>(res.ptr - buf.data())};
}

std::string_view render_ull(std::array<char, 24>& buf,
                             unsigned long long v) noexcept {
    auto res = std::to_chars(buf.data(), buf.data() + buf.size(), v);
    if (res.ec != std::errc{}) return std::string_view{};
    return std::string_view{buf.data(),
        static_cast<std::size_t>(res.ptr - buf.data())};
}

}  // namespace

extern "C" int pvpgn_v3_bnetd_timerlist_create_try(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_bnetd_timer_bridge",
        "timerlist create observed",
        {});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_timerlist_destroy_try(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_bnetd_timer_bridge",
        "timerlist destroy observed",
        {});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_timerlist_add_timer_try(
    int sd,
    unsigned long long when) noexcept {
    std::array<char, 24> sdbuf{};
    std::array<char, 24> wbuf{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"sd",   render_int(sdbuf, sd)},
        {"when", render_ull(wbuf,  when)},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Trace,
        "v3_bnetd_timer_bridge",
        "timerlist add_timer observed",
        {fields[0], fields[1]});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_timerlist_del_all_timers_try(
    int sd) noexcept {
    std::array<char, 24> sdbuf{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"sd", render_int(sdbuf, sd)},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_bnetd_timer_bridge",
        "timerlist del_all_timers observed",
        {fields[0]});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_timerlist_check_timers_try(
    unsigned long long when) noexcept {
    std::array<char, 24> wbuf{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"when", render_ull(wbuf, when)},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Trace,
        "v3_bnetd_timer_bridge",
        "timerlist check_timers observed",
        {fields[0]});
    return 0;
}
