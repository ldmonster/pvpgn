// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_creategamereply_bridge.hpp
/// Strangler-fig hook for D2CS_CLIENT_CREATEGAMEREPLY (type 0x03).
/// Two call sites: handle_d2cs.cpp (failure path) and handle_d2gs.cpp
/// (success/fail dispatch from gs).

extern "C" {

/// Build a D2CS_CLIENT_CREATEGAMEREPLY (10-byte body: seqno u16,
/// gameid u16, u1 u16, reply u32) via the v3 codec.
///
/// Returns:
///   * 1 -- handled (caller MUST skip legacy emit + outqueue push).
///   * 0 -- not handled (legacy fallback runs).
int pvpgn_v3_d2cs_send_creategamereply(void* conn_ptr,
                                        unsigned int seqno,
                                        unsigned int gameid,
                                        unsigned int u1,
                                        unsigned int reply) noexcept;

}  // extern "C"
