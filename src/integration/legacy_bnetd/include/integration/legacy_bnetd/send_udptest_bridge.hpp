// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_udptest_bridge.hpp
/// Strangler-fig observation hook for SERVER_UDPTEST (UDP packet_class_udp)
/// outbound sends used by `udptest_send` in `udptest_send.cpp`.
///
/// Wire layout (packet_class_udp):
///   [udp header 4 bytes]
///   bnettag : uint32 (big-endian tag "BNET")
///
/// This packet is sent via raw UDP (`psock_sendto`), NOT via
/// `conn_push_outqueue`.  The bridge is therefore observation-only: it always
/// returns 0, causing the legacy UDP send path to run unchanged.
///
/// When the v3 UDP test path is implemented, this function can be upgraded to
/// a proper encode bridge.
///
/// Returns:
///   0  -> always (observation-only; fall back to legacy UDP send).

extern "C" {

/// Observation hook for SERVER_UDPTEST.
/// Called once per UDP test packet attempt inside `udptest_send`.
///
/// @param conn_ptr  Opaque legacy `t_connection*`. nullptr -> returns 0.
int pvpgn_v3_observe_udptest(void* conn_ptr) noexcept;

}  // extern "C"
