// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_loginreply_bridge.hpp
/// Strangler-fig hook for D2CS_CLIENT_LOGINREPLY (type 0x01). Covers
/// the single push at the end of `on_bnetd_accountloginreply` in
/// `src/d2cs/handle_bnetd.cpp`.

extern "C" {

/// Build a D2CS_CLIENT_LOGINREPLY via the v3 codec and dispatch via
/// the registered legacy_d2cs send-packet handler.
///
/// Returns:
///   * 1 -- handled (caller MUST skip legacy emit + outqueue push).
///   * 0 -- not handled (legacy fallback runs).
int pvpgn_v3_d2cs_send_loginreply(void* conn_ptr,
                                   unsigned int reply) noexcept;

}  // extern "C"
