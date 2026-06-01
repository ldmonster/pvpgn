// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_charlistreply_bridge.hpp
/// Strangler-fig hook for `D2CS_CLIENT_CHARLISTREPLY` (0x17).
///
/// Legacy emitter: `on_client_charlistreq` in
/// `src/d2cs/handle_d2cs.cpp`. The v3 bridge takes a flat entry array
/// (caller-sorted ASC/DESC) and the resolved `maxchar_field` value
/// (legacy: actual `maxchar` if `allow_newchar` and room remains,
/// otherwise 0).

#include <cstdint>

extern "C" {

struct pvpgn_v3_d2cs_charlist_entry {
    char const*          charname;       // NUL-terminated, required
    unsigned char const* portrait;       // raw portrait bytes
    unsigned int         portrait_len;   // portrait byte count (no NUL)
};

int pvpgn_v3_d2cs_send_charlistreply(
    void*                                  conn_ptr,
    unsigned int                           maxchar_field,
    pvpgn_v3_d2cs_charlist_entry const*    entries,
    unsigned int                           count) noexcept;

}  // extern "C"
