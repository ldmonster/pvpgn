// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_ladderreply_bridge.hpp
/// Strangler-fig hook for `D2CS_CLIENT_LADDERREPLY` (0x11).
///
/// The legacy emitter (`d2cs_send_client_ladder` in
/// `src/d2cs/handle_d2cs.cpp`) walks `d2ladder` state and pushes one
/// or more 0x11 packets onto `conn_d2cs_outqueue`. The v3 replacement
/// takes a flat array of ladder entries already extracted by the
/// caller and emits the same byte sequences via the
/// `pvpgn_v3_d2cs_send_packet_try` bridge.

#include <cstdint>

extern "C" {

/// One ladder ranking entry. Mirrors the legacy on-wire layout.
struct pvpgn_v3_d2cs_ladder_entry {
    std::uint32_t exp_low;
    std::uint32_t exp_high;
    std::uint16_t status;
    std::uint8_t  level;
    std::uint8_t  u1;
    char          charname[16];
};

/// Emit the full ladder reply, splitting into multiple 0x11 packets as
/// needed. `type` is the legacy ladder-type byte (e.g. amazon, sorc).
/// Returns 1 on success (all packets accepted by the sink), 0 if the
/// underlying send_packet bridge declined any packet.
int pvpgn_v3_d2cs_send_ladderreply(void* conn_ptr,
                                    unsigned int type,
                                    unsigned int start_pos,
                                    pvpgn_v3_d2cs_ladder_entry const* entries,
                                    unsigned int count) noexcept;

}  // extern "C"
