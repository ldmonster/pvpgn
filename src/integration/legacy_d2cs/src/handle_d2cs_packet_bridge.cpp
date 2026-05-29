// SPDX-License-Identifier: GPL-2.0-or-later
//
// R235(1): observation bridge for d2cs client-packet dispatch.

#include "integration/legacy_d2cs/handle_d2cs_packet_bridge.hpp"

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

extern "C" int pvpgn_v3_d2cs_handle_d2cs_packet_try(
    int sd,
    unsigned int packet_type,
    unsigned int packet_size) noexcept {
    std::array<char, 20> sdbuf{};
    std::array<char, 20> ptbuf{};
    std::array<char, 20> psbuf{};
    const pvpgn::core::ILogger::Field fields[] = {
        {"sd",          render_int (sdbuf, sd)},
        {"packet_type", render_uint(ptbuf, packet_type)},
        {"packet_size", render_uint(psbuf, packet_size)},
    };
    plc::bridge_log_kv(pvpgn::core::LogLevel::Trace,
        "v3_d2cs_handle_d2cs_bridge",
        "client packet dispatched",
        {fields[0], fields[1], fields[2]});
    return 0;
}
