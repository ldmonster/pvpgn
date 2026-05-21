// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_w3route_bridge.hpp"

// All eight w3route packet types are observation-only bridges.
// They always return 0, causing the legacy code path to run unchanged.
//
// These packets involve complex per-player coordination data (handles,
// usernames, race, addresses, level info) and are sent to w3route connections.
// The v3 w3route encoder does not yet have full support for all these packet
// types.  When the encoder is complete, each function can be upgraded to
// encode and send the packet via pvpgn_v3_send_packet_try.

extern "C" int pvpgn_v3_observe_w3route_echoreq(void* /*conn_ptr*/,
                                                  unsigned int /*ticks*/) noexcept {
    return 0;
}

extern "C" int pvpgn_v3_observe_w3route_ack(void* /*conn_ptr*/) noexcept {
    return 0;
}

extern "C" int pvpgn_v3_observe_w3route_loadingack(void* /*conn_ptr*/) noexcept {
    return 0;
}

extern "C" int pvpgn_v3_observe_w3route_ready(void* /*conn_ptr*/) noexcept {
    return 0;
}

extern "C" int pvpgn_v3_observe_w3route_playerinfo(void* /*conn_ptr*/) noexcept {
    return 0;
}

extern "C" int pvpgn_v3_observe_w3route_levelinfo(void* /*conn_ptr*/) noexcept {
    return 0;
}

extern "C" int pvpgn_v3_observe_w3route_startgame1(void* /*conn_ptr*/) noexcept {
    return 0;
}

extern "C" int pvpgn_v3_observe_w3route_startgame2(void* /*conn_ptr*/) noexcept {
    return 0;
}
