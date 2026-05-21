// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_outbound_obs_bridges.hpp
/// Strangler-fig observation hooks for D2CS outbound packets that are
/// sent to bnetd or d2gs (not to the client).  These are complex
/// internal-protocol packets whose v3 encoders are not yet complete;
/// each bridge returns 0 (no-handler / fall-through) so the legacy
/// path continues to run unchanged.
///
/// Covered packet types:
///   D2CS_BNETD_ACCOUNTLOGINREQ (0x01) — d2cs → bnetd
///   D2CS_BNETD_CHARLOGINREQ    (0x03) — d2cs → bnetd
///   D2CS_D2GS_CREATEGAMEREQ    (0x01) — d2cs → d2gs
///   D2CS_D2GS_JOINGAMEREQ      (0x02) — d2cs → d2gs
///   D2CS_D2GS_ECHOREQ          (0x05) — d2cs → d2gs (keepalive)
///   D2CS_D2GS_CONTROL          (0x06) — d2cs → d2gs (restart)

#include <cstdint>

extern "C" {

/// D2CS_BNETD_ACCOUNTLOGINREQ: observation hook (always returns 0).
int pvpgn_v3_d2cs_obs_accountloginreq_bnetd(void* conn_ptr) noexcept;

/// D2CS_BNETD_CHARLOGINREQ: observation hook (always returns 0).
int pvpgn_v3_d2cs_obs_charloginreq_bnetd(void* conn_ptr) noexcept;

/// D2CS_D2GS_CREATEGAMEREQ: observation hook (always returns 0).
int pvpgn_v3_d2cs_obs_creategamereq_d2gs(void* conn_ptr) noexcept;

/// D2CS_D2GS_JOINGAMEREQ: observation hook (always returns 0).
int pvpgn_v3_d2cs_obs_joingamereq_d2gs(void* conn_ptr) noexcept;

/// D2CS_D2GS_ECHOREQ: observation hook (always returns 0).
int pvpgn_v3_d2cs_obs_echoreq_d2gs(void* conn_ptr) noexcept;

/// D2CS_D2GS_CONTROL: observation hook (always returns 0).
int pvpgn_v3_d2cs_obs_control_d2gs(void* conn_ptr) noexcept;

/// D2CS_CLIENT_LADDERREPLY: observation hook (always returns 0).
/// Used at the d2cs_send_client_ladder() call sites until the v3
/// encoder is wired to the legacy ladder data structures.
int pvpgn_v3_d2cs_obs_ladderreply(void* conn_ptr) noexcept;

/// D2CS_CLIENT_CHARLISTREPLY / CHARLISTREPLY_110: observation hook (always returns 0).
/// Used at the on_client_charlistreq() call sites until the v3
/// encoder is wired to the legacy charlist data structures.
int pvpgn_v3_d2cs_obs_charlistreply(void* conn_ptr) noexcept;

}  // extern "C"
