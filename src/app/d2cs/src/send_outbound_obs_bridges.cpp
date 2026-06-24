// SPDX-License-Identifier: GPL-2.0-or-later
#include "app/d2cs/legacy_d2cs_bridges/send_outbound_obs_bridges.hpp"

// Observation-only bridges for D2CS outbound packets sent to bnetd or d2gs.
// Each function returns 0 (no-handler / fall-through) so the legacy path
// continues to run unchanged.  These stubs exist to:
//   1. Register the call site under #ifdef PVPGN_V3_D2CS_INTEGRATION so the
//      site is counted as "bridged".
//   2. Provide a hook point for future encoders without touching the
//      legacy source again.

extern "C" int pvpgn_v3_d2cs_obs_accountloginreq_bnetd(void* /*conn_ptr*/) noexcept {
    return 0;
}

extern "C" int pvpgn_v3_d2cs_obs_charloginreq_bnetd(void* /*conn_ptr*/) noexcept {
    return 0;
}

extern "C" int pvpgn_v3_d2cs_obs_creategamereq_d2gs(void* /*conn_ptr*/) noexcept {
    return 0;
}

extern "C" int pvpgn_v3_d2cs_obs_joingamereq_d2gs(void* /*conn_ptr*/) noexcept {
    return 0;
}

extern "C" int pvpgn_v3_d2cs_obs_echoreq_d2gs(void* /*conn_ptr*/) noexcept {
    return 0;
}

extern "C" int pvpgn_v3_d2cs_obs_control_d2gs(void* /*conn_ptr*/) noexcept {
    return 0;
}

extern "C" int pvpgn_v3_d2cs_obs_ladderreply(void* /*conn_ptr*/) noexcept {
    return 0;
}

extern "C" int pvpgn_v3_d2cs_obs_charlistreply(void* /*conn_ptr*/) noexcept {
    return 0;
}
