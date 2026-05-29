// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_charloginreply_bridge.hpp
/// Strangler-fig hook for D2CS_CLIENT_CHARLOGINREPLY (type 0x07).
/// Single call site in handle_bnetd.cpp on_bnetd_charloginreply,
/// CLIENT_D2CS_CHARLOGINREQ branch.

extern "C" {

/// Build a D2CS_CLIENT_CHARLOGINREPLY via the v3 codec and dispatch
/// via the registered legacy_d2cs send-packet handler.
///
/// Returns:
///   * 1 -- handled (caller MUST skip legacy emit + outqueue push).
///   * 0 -- not handled (legacy fallback runs).
int pvpgn_v3_d2cs_send_charloginreply(void* conn_ptr,
                                       unsigned int reply) noexcept;

}  // extern "C"
