// SPDX-License-Identifier: GPL-2.0-or-later
//
// R234(3): observation bridge for d2cs per-connection teardown.

#include "integration/legacy_d2cs/conn_bridge.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <string_view>
#include <system_error>

#include "core/logging.hpp"
#include "integration/legacy_d2cs/bridge_logger.hpp"

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

extern "C" int pvpgn_v3_d2cs_conn_destroy_try(
    int sd,
    unsigned int sessionnum,
    unsigned int cclass,
    unsigned int state) noexcept {
    std::array<char, 20> sdbuf{};
    std::array<char, 20> snbuf{};
    std::array<char, 20> ccbuf{};
    std::array<char, 20> stbuf{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"sd",         render_int (sdbuf, sd)},
        {"sessionnum", render_uint(snbuf, sessionnum)},
        {"cclass",     render_uint(ccbuf, cclass)},
        {"state",      render_uint(stbuf, state)},
    };
    plc::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_d2cs_conn_bridge",
        "conn destroy observed",
        {fields[0], fields[1], fields[2], fields[3]});
    return 0;
}
