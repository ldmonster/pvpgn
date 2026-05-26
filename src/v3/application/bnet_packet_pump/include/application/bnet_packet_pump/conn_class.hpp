// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file conn_class.hpp
/// Pure-C++ enum mirroring the legacy `conn_class_*` family in
/// `src/bnetd/connection.h`. Used by the v3 bnet packet pump
/// (R179.a scaffold) so dispatch decisions can be expressed in
/// terms of the enum rather than the legacy `int` constants.
///
/// Values match the legacy `t_conn_class` ordering so the v3 pump
/// can interop with `conn_get_class()` without a translation
/// table when this scaffold is later wired into
/// `legacy_bnet_frame_router_link.cpp`.

#include <cstdint>
#include <string_view>

namespace pvpgn::application::bnet_packet_pump {

/// Connection-class enum mirroring legacy `conn_class_*`. Wire
/// values come from the `CLIENT_INITCONN_CLASS_*` byte received
/// in the very first packet (see `protocol/bnet/init_codec.hpp`)
/// PLUS a small set of "synthetic" classes that the legacy
/// connection machinery transitions into after the init handshake
/// (`bnet`, `irc`, `w3route`, etc).
enum class ConnClass : std::uint8_t {
    kNone        = 0,
    kInit        = 1,   ///< Awaiting the single cclass byte.
    kBnet        = 2,
    kFile        = 3,
    kBot         = 4,
    kTelnet      = 5,
    kIrc         = 6,
    kD2cs        = 7,
    kD2csBnetd   = 8,
    kW3route     = 9,
    kWol         = 10,
    kWolGameres  = 11,
    kWgameres    = 12,
    kWserv       = 13,
    kApiReg      = 14,
    kAuthReq     = 15,
};

/// Compact name useful for log lines / `bridge_log` calls.
constexpr std::string_view to_string(ConnClass c) noexcept {
    switch (c) {
        case ConnClass::kNone:       return "none";
        case ConnClass::kInit:       return "init";
        case ConnClass::kBnet:       return "bnet";
        case ConnClass::kFile:       return "file";
        case ConnClass::kBot:        return "bot";
        case ConnClass::kTelnet:     return "telnet";
        case ConnClass::kIrc:        return "irc";
        case ConnClass::kD2cs:       return "d2cs";
        case ConnClass::kD2csBnetd:  return "d2cs_bnetd";
        case ConnClass::kW3route:    return "w3route";
        case ConnClass::kWol:        return "wol";
        case ConnClass::kWolGameres: return "wolgameres";
        case ConnClass::kWgameres:   return "wgameres";
        case ConnClass::kWserv:      return "wserv";
        case ConnClass::kApiReg:     return "apireg";
        case ConnClass::kAuthReq:    return "authreq";
    }
    return "?";
}

}  // namespace pvpgn::application::bnet_packet_pump
