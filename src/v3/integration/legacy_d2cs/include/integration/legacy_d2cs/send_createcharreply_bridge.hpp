// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_createcharreply_bridge.hpp
/// Strangler-fig hook for D2CS_CLIENT_CREATECHARREPLY (type 0x02).
/// Two call sites: handle_d2cs.cpp (direct client request reply) and
/// handle_bnetd.cpp (bnetd async reply forwarding).

extern "C" {

/// Build a D2CS_CLIENT_CREATECHARREPLY via the v3 codec and dispatch
/// via the registered legacy_d2cs send-packet handler.
///
/// Returns:
///   * 1 -- handled (caller MUST skip legacy emit + outqueue push).
///   * 0 -- not handled (legacy fallback runs).
int pvpgn_v3_d2cs_send_createcharreply(void* conn_ptr,
                                        unsigned int reply) noexcept;

}  // extern "C"
