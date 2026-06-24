// SPDX-License-Identifier: GPL-2.0-or-later
//
// Observation bridge for d2cs init-packet dispatch.

#include "app/d2cs/legacy_d2cs_bridges/handle_init_bridge.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <string_view>
#include <system_error>

#include "core/logging.hpp"
#include "app/d2cs/legacy_d2cs_bridges/bridge_logger.hpp"

namespace plc = pvpgn::integration::legacy_d2cs;

namespace {

std::string_view render_int(std::array<char, 20>& buf, int v) noexcept {
    auto res = std::to_chars(buf.data(), buf.data() + buf.size(), v);
    if (res.ec != std::errc{}) return std::string_view{};
    return std::string_view{buf.data(),
        static_cast<std::size_t>(res.ptr - buf.data())};
}

std::string_view render_uint(std::array<char, 20>& buf,
                              unsigned int v) noexcept {
    auto res = std::to_chars(buf.data(), buf.data() + buf.size(), v);
    if (res.ec != std::errc{}) return std::string_view{};
    return std::string_view{buf.data(),
        static_cast<std::size_t>(res.ptr - buf.data())};
}

}  // namespace

extern "C" int pvpgn_v3_d2cs_handle_init_packet(
    int sd,
    unsigned int cclass) noexcept {
    std::array<char, 20> sdbuf{};
    std::array<char, 20> ccbuf{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"sd",     render_int (sdbuf, sd)},
        {"cclass", render_uint(ccbuf, cclass)},
    };
    plc::bridge_log_kv(pvpgn::core::LogLevel::Trace,
        "v3_d2cs_handle_init_bridge",
        "init packet observed",
        {fields[0], fields[1]});
    return 0;
}

// Observation bridge for d2gs initconn classification.
extern "C" int pvpgn_v3_d2cs_on_d2gs_initconn(
    int sd,
    unsigned int addr) noexcept {
    std::array<char, 20> sdbuf{};
    std::array<char, 20> addrbuf{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"sd",   render_int (sdbuf,   sd)},
        {"addr", render_uint(addrbuf, addr)},
    };
    plc::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_d2cs_handle_init_bridge",
        "on d2gs initconn observed",
        {fields[0], fields[1]});
    return 0;
}

// Observation bridge for d2cs initconn classification.
extern "C" int pvpgn_v3_d2cs_on_d2cs_initconn(
    int sd) noexcept {
    std::array<char, 20> sdbuf{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"sd", render_int(sdbuf, sd)},
    };
    plc::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_d2cs_handle_init_bridge",
        "on d2cs initconn observed",
        {fields[0]});
    return 0;
}
