// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_d2cs_bnetd_bridges.hpp"

// All five bnetd→D2CS packet types are observation-only bridges.
// They always return 0, causing the legacy code path to run unchanged.
//
// These packets are sent over the bnetd↔D2CS link protocol using
// packet_class_d2cs_bnetd.  The v3 D2CS-bnetd link encoder is not yet
// complete.  When the encoder is ready, each function can be upgraded to
// encode and send the packet via pvpgn_v3_send_packet_try.
//
// Packet types:
//   BNETD_D2CS_AUTHREQ          - bnetd initiates auth handshake with d2cs
//   BNETD_D2CS_AUTHREPLY        - bnetd replies to d2cs auth response
//   BNETD_D2CS_ACCOUNTLOGINREPLY - bnetd replies to d2cs account login request
//   BNETD_D2CS_CHARLOGINREPLY   - bnetd replies to d2cs character login request
//   BNETD_D2CS_GAMEINFOREQ      - bnetd requests game info from d2cs

extern "C" int pvpgn_v3_observe_d2cs_bnetd_authreq(
        void* /*conn_ptr*/,
        unsigned int /*sessionnum*/) noexcept {
    return 0;
}

extern "C" int pvpgn_v3_observe_d2cs_bnetd_authreply(
        void* /*conn_ptr*/,
        unsigned int /*reply*/) noexcept {
    return 0;
}

extern "C" int pvpgn_v3_observe_d2cs_bnetd_accountloginreply(
        void*        /*conn_ptr*/,
        unsigned int /*seqno*/,
        unsigned int /*reply*/) noexcept {
    return 0;
}

extern "C" int pvpgn_v3_observe_d2cs_bnetd_charloginreply(
        void*        /*conn_ptr*/,
        unsigned int /*seqno*/,
        unsigned int /*reply*/) noexcept {
    return 0;
}

extern "C" int pvpgn_v3_observe_d2cs_bnetd_gameinforeq(
        void*       /*conn_ptr*/,
        const char* /*gamename*/) noexcept {
    return 0;
}
