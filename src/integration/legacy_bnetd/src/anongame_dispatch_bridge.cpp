// SPDX-License-Identifier: GPL-2.0-or-later
//
// Observation bridge for SID_FINDANONGAME (0x44) dispatch. See
// header.

#include "integration/legacy_bnetd/anongame_dispatch_bridge.hpp"

#include <cstdio>
#include <string>
#include <string_view>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"

namespace plb = pvpgn::integration::legacy_bnetd;

namespace {

// Mirrors the sub-option constants in
// `src/common/anongame_protocol.h` (CLIENT_FINDANONGAME_* / 
// CLIENT_ANONGAME_TOURNAMENT).
const char* option_name(unsigned int o) noexcept {
    switch (o) {
        case 0x00: return "SEARCH";
        case 0x02: return "INFOS";
        case 0x03: return "CANCEL";
        case 0x04: return "PROFILE";
        case 0x05: return "AT_SEARCH";
        case 0x06: return "AT_INVITER_SEARCH";
        case 0x07: return "TOURNAMENT";
        case 0x08: return "PROFILE_CLAN";
        case 0x09: return "GET_ICON";
        case 0x0A: return "SET_ICON";
        default:   return "?";
    }
}

std::string to_hex8(unsigned int v) {
    char buf[2 + 2 + 1];
    std::snprintf(buf, sizeof(buf), "0x%02x", v & 0xffu);
    return std::string{buf};
}

}  // namespace

extern "C" int pvpgn_v3_anongame_dispatch(void* conn_ptr,
                                              unsigned int option) noexcept {
    if (conn_ptr == nullptr) return 0;

    const std::string raw_str = to_hex8(option);
    const pvpgn::core::ILogger::Field fields[] = {
        {"option", std::string_view{option_name(option)}},
        {"raw",    std::string_view{raw_str}},
    };
    plb::bridge_log_kv(pvpgn::core::LogLevel::Debug,
        "v3_anongame_dispatch_bridge",
        "anongame dispatch observed",
        {fields[0], fields[1]});

    return 0;
}
