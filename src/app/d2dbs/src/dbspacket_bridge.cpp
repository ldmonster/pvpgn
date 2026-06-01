// SPDX-License-Identifier: GPL-2.0-or-later
//
// R242: observation bridges for d2dbs dbspacket.cpp dispatchers.

#include "integration/legacy_d2dbs/dbspacket_bridge.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <string_view>
#include <system_error>

#include "core/logging.hpp"
#include "integration/legacy_d2dbs/bridge_logger.hpp"

namespace pld = pvpgn::integration::legacy_d2dbs;

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

extern "C" int pvpgn_v3_d2dbs_packet_handle(
    int sd,
    unsigned int stats,
    unsigned int type) noexcept {
    std::array<char, 20> sdbuf{};
    std::array<char, 20> stbuf{};
    std::array<char, 20> tybuf{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"sd",    render_int (sdbuf, sd)},
        {"stats", render_uint(stbuf, stats)},
        {"type",  render_uint(tybuf, type)},
    };
    pld::bridge_log_kv(pvpgn::core::LogLevel::Trace,
        "v3_d2dbs_dbspacket_bridge",
        "packet handle observed",
        {fields[0], fields[1], fields[2]});
    return 0;
}

extern "C" int pvpgn_v3_d2dbs_check_timeout(void) noexcept {
    pld::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_d2dbs_dbspacket_bridge",
        "check_timeout observed",
        {});
    return 0;
}

extern "C" int pvpgn_v3_d2dbs_keepalive(void) noexcept {
    pld::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_d2dbs_dbspacket_bridge",
        "keepalive observed",
        {});
    return 0;
}
