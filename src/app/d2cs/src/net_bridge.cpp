// SPDX-License-Identifier: GPL-2.0-or-later
//
// Observation bridges for d2cs net.cpp socket helpers.

#include "app/d2cs/legacy_d2cs_bridges/net_bridge.hpp"

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

extern "C" int pvpgn_v3_d2cs_net_socket(int type) noexcept {
    std::array<char, 20> tbuf{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"type", render_int(tbuf, type)},
    };
    plc::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_d2cs_net_bridge",
        "net socket observed",
        {fields[0]});
    return 0;
}

extern "C" int pvpgn_v3_d2cs_net_check_connected(int sock) noexcept {
    std::array<char, 20> sbuf{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"sock", render_int(sbuf, sock)},
    };
    plc::bridge_log_kv(pvpgn::core::LogLevel::Trace,
        "v3_d2cs_net_bridge",
        "net check_connected observed",
        {fields[0]});
    return 0;
}

extern "C" int pvpgn_v3_d2cs_net_listen(
    unsigned int ip,
    unsigned int port,
    int type) noexcept {
    std::array<char, 20> ipbuf{};
    std::array<char, 20> pbuf{};
    std::array<char, 20> tbuf{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"ip",   render_uint(ipbuf, ip)},
        {"port", render_uint(pbuf,  port)},
        {"type", render_int (tbuf,  type)},
    };
    plc::bridge_log_kv(pvpgn::core::LogLevel::Info,
        "v3_d2cs_net_bridge",
        "net listen observed",
        {fields[0], fields[1], fields[2]});
    return 0;
}
