// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_d2cs_bnetd_bridges.hpp
/// Strangler-fig observation hooks for `packet_class_d2cs_bnetd` outbound
/// sends in `src/bnetd/handle_d2cs.cpp`.
///
/// These five packet types are sent from bnetd to the D2CS server over the
/// bnetd↔D2CS link protocol.  They carry authentication, account-login,
/// character-login, and game-info request/reply data.  The v3 D2CS-bnetd
/// link encoder is not yet complete, so all bridges here are
/// **observation-only**: they always return 0, causing the legacy code path
/// to run unchanged.
///
/// Packet types covered:
///   - BNETD_D2CS_AUTHREQ          (bnetd→d2cs: initial auth request)
///   - BNETD_D2CS_AUTHREPLY        (bnetd→d2cs: auth reply after d2cs auth)
///   - BNETD_D2CS_ACCOUNTLOGINREPLY (bnetd→d2cs: account login reply)
///   - BNETD_D2CS_CHARLOGINREPLY   (bnetd→d2cs: character login reply)
///   - BNETD_D2CS_GAMEINFOREQ      (bnetd→d2cs: game info request)
///
/// Returns (all functions):
///   0  -> always (no v3 handler for these packet types yet; fall back to legacy).

extern "C" {

/// Observation hook for BNETD_D2CS_AUTHREQ.
/// Sent by `handle_d2cs_init` when bnetd initiates the D2CS auth handshake.
/// @param conn_ptr   Opaque legacy `t_connection*` (d2cs link connection).
/// @param sessionnum Session number sent in the auth request.
int pvpgn_v3_observe_d2cs_bnetd_authreq(void* conn_ptr,
                                          unsigned int sessionnum) noexcept;

/// Observation hook for BNETD_D2CS_AUTHREPLY.
/// Sent by `on_d2cs_authreply` after verifying the D2CS auth response.
/// @param conn_ptr  Opaque legacy `t_connection*` (d2cs link connection).
/// @param reply     Reply code (BNETD_D2CS_AUTHREPLY_SUCCEED / _BAD_VERSION).
int pvpgn_v3_observe_d2cs_bnetd_authreply(void* conn_ptr,
                                            unsigned int reply) noexcept;

/// Observation hook for BNETD_D2CS_ACCOUNTLOGINREPLY.
/// Sent by `on_d2cs_accountloginreq` after verifying account credentials.
/// @param conn_ptr  Opaque legacy `t_connection*` (d2cs link connection).
/// @param seqno     Sequence number echoed from the request.
/// @param reply     Reply code (BNETD_D2CS_ACCOUNTLOGINREPLY_SUCCEED / _FAILED).
int pvpgn_v3_observe_d2cs_bnetd_accountloginreply(void*        conn_ptr,
                                                    unsigned int seqno,
                                                    unsigned int reply) noexcept;

/// Observation hook for BNETD_D2CS_CHARLOGINREPLY.
/// Sent by `on_d2cs_charloginreq` after verifying character login.
/// @param conn_ptr  Opaque legacy `t_connection*` (d2cs link connection).
/// @param seqno     Sequence number echoed from the request.
/// @param reply     Reply code (BNETD_D2CS_CHARLOGINREPLY_SUCCEED / _FAILED).
int pvpgn_v3_observe_d2cs_bnetd_charloginreply(void*        conn_ptr,
                                                 unsigned int seqno,
                                                 unsigned int reply) noexcept;

/// Observation hook for BNETD_D2CS_GAMEINFOREQ.
/// Sent by `send_d2cs_gameinforeq` to request game info from D2CS.
/// @param conn_ptr  Opaque legacy `t_connection*` (realm's d2cs link connection).
/// @param gamename  NUL-terminated game name string appended to the packet.
int pvpgn_v3_observe_d2cs_bnetd_gameinforeq(void*       conn_ptr,
                                              const char* gamename) noexcept;

}  // extern "C"
