// SPDX-License-Identifier: GPL-2.0-or-later
//
// Observation bridges for d2dbs server main + per-connection
// shutdown.

#include "app/d2dbs/legacy_d2dbs_bridges/dbserver_bridge.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <string_view>
#include <system_error>

#include "core/logging.hpp"
#include "app/d2dbs/legacy_d2dbs_bridges/bridge_logger.hpp"

namespace pld = pvpgn::integration::legacy_d2dbs;

namespace {

std::string_view render_uint(std::array<char, 20>& buf,
                              unsigned int v) noexcept {
    auto res = std::to_chars(buf.data(), buf.data() + buf.size(), v);
    if (res.ec != std::errc{}) {
        return std::string_view{};
    }
    return std::string_view{buf.data(),
        static_cast<std::size_t>(res.ptr - buf.data())};
}

std::string_view render_int(std::array<char, 20>& buf, int v) noexcept {
    auto res = std::to_chars(buf.data(), buf.data() + buf.size(), v);
    if (res.ec != std::errc{}) {
        return std::string_view{};
    }
    return std::string_view{buf.data(),
        static_cast<std::size_t>(res.ptr - buf.data())};
}

}  // namespace

extern "C" int pvpgn_v3_d2dbs_server_main(void) noexcept {
    pld::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_d2dbs_server_bridge",
        "server main entry observed",
        {});
    return 0;
}

extern "C" int pvpgn_v3_d2dbs_server_shutdown_connection(
    int sd,
    unsigned int serverid,
    unsigned int conn_type,
    unsigned int verified) noexcept {
    std::array<char, 20> sdbuf{};
    std::array<char, 20> idbuf{};
    std::array<char, 20> tybuf{};
    std::array<char, 20> vbuf{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"sd",       render_int(sdbuf, sd)},
        {"serverid", render_uint(idbuf, serverid)},
        {"type",     render_uint(tybuf, conn_type)},
        {"verified", render_uint(vbuf,  verified)},
    };
    pld::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_d2dbs_server_bridge",
        "server shutdown_connection observed",
        {fields[0], fields[1], fields[2], fields[3]});
    return 0;
}
