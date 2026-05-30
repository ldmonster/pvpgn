// SPDX-License-Identifier: GPL-2.0-or-later
// R244: observation bridges for bnetd IP-ban subsystem.

#include "integration/legacy_bnetd/ipban_bridge.hpp"

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

std::string_view render_ull(std::array<char, 24>& buf,
                             unsigned long long v) noexcept {
    auto res = std::to_chars(buf.data(), buf.data() + buf.size(), v);
    if (res.ec != std::errc{}) return std::string_view{};
    return std::string_view{buf.data(),
        static_cast<std::size_t>(res.ptr - buf.data())};
}

constexpr const char* kModule = "v3_bnetd_ipban_bridge";

}  // namespace

extern "C" int pvpgn_v3_bnetd_ipban_create(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        kModule, "ipbanlist create observed", {});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_ipban_destroy(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        kModule, "ipbanlist destroy observed", {});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_ipban_load(
    const char* filename) noexcept {
    const pvpgn::core::ILogger::Field fields[] = {
        {"filename", safe_str(filename)},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        kModule, "ipbanlist load observed",
        {fields[0]});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_ipban_save(
    const char* filename) noexcept {
    const pvpgn::core::ILogger::Field fields[] = {
        {"filename", safe_str(filename)},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        kModule, "ipbanlist save observed",
        {fields[0]});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_ipban_check(
    const char* ipaddr) noexcept {
    const pvpgn::core::ILogger::Field fields[] = {
        {"ipaddr", safe_str(ipaddr)},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Trace,
        kModule, "ipbanlist check observed",
        {fields[0]});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_ipban_add(
    int sd,
    const char* ipaddr,
    unsigned long long endtime) noexcept {
    std::array<char, 24> sdbuf{};
    std::array<char, 24> ebuf{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"sd",      render_int(sdbuf, sd)},
        {"ipaddr",  safe_str(ipaddr)},
        {"endtime", render_ull(ebuf, endtime)},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Info,
        kModule, "ipbanlist add observed",
        {fields[0], fields[1], fields[2]});
    return 0;
}

extern "C" int pvpgn_v3_bnetd_ipban_unload_expired(void) noexcept {
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        kModule, "ipbanlist unload_expired observed", {});
    return 0;
}
