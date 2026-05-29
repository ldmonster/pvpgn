// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_anongame_found_bridge.hpp"

// SERVER_ANONGAME_FOUND is a complex variable-length packet carrying IP, port,
// player index, queue, game id, type, gametype, a variable-length map name
// string, and a pt2 data block.  The v3 anongame encoder does not yet have
// full support for this packet type, so this bridge is observation-only.
//
// Returning 0 causes the legacy code path to run unchanged.  When the v3
// encoder is complete, this function can be upgraded to encode and send the
// packet via pvpgn_v3_send_packet_try.

extern "C" int pvpgn_v3_observe_anongame_found(void* /*conn_ptr*/) noexcept {
    // Observation-only: always fall back to legacy.
    return 0;
}
