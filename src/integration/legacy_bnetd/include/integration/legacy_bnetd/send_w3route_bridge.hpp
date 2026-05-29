// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_w3route_bridge.hpp
/// Strangler-fig hooks for packet_class_w3route outbound sends used by
/// `conn_test_latency` in `connection.cpp`, `handle_w3route_packet` and
/// `handle_anongame_join` in `anongame.cpp`.
///
/// The eight w3route packet types covered here are:
///   - SERVER_W3ROUTE_ECHOREQ    (latency ping sent by conn_test_latency)
///   - SERVER_W3ROUTE_ACK        (route connection acknowledgement)
///   - SERVER_W3ROUTE_LOADINGACK (per-player loading acknowledgement)
///   - SERVER_W3ROUTE_READY      (all-players-loaded signal)
///   - SERVER_W3ROUTE_PLAYERINFO (per-player info: handle, username, race, addrs)
///   - SERVER_W3ROUTE_LEVELINFO  (per-player level info)
///   - SERVER_W3ROUTE_STARTGAME1 (start-game signal 1)
///   - SERVER_W3ROUTE_STARTGAME2 (start-game signal 2)
///
/// These packets involve complex per-player coordination data and are sent to
/// w3route connections (not the main bnet connection).  The v3 w3route encoder
/// does not yet have full support for all these packet types, so all bridges
/// here are observation-only: they always return 0, causing the legacy code
/// path to run unchanged.
///
/// When the v3 w3route encoder is complete, these functions can be upgraded to
/// proper encode bridges.
///
/// Returns (all functions):
///   0  -> always (no v3 handler for these packet types yet; fall back to legacy).

extern "C" {

/// Observation hook for SERVER_W3ROUTE_ECHOREQ.
/// Sent by `conn_test_latency` in `connection.cpp` to ping w3route clients.
/// @param conn_ptr  Opaque legacy `t_connection*` (w3route connection).
/// @param ticks     Current tick count sent in the echo request.
int pvpgn_v3_observe_w3route_echoreq(void* conn_ptr, unsigned int ticks) noexcept;

/// Observation hook for SERVER_W3ROUTE_ACK.
/// @param conn_ptr  Opaque legacy `t_connection*` (w3route connection).
int pvpgn_v3_observe_w3route_ack(void* conn_ptr) noexcept;

/// Observation hook for SERVER_W3ROUTE_LOADINGACK.
/// @param conn_ptr  Opaque legacy `t_connection*` (w3route connection).
int pvpgn_v3_observe_w3route_loadingack(void* conn_ptr) noexcept;

/// Observation hook for SERVER_W3ROUTE_READY.
/// @param conn_ptr  Opaque legacy `t_connection*` (w3route connection).
int pvpgn_v3_observe_w3route_ready(void* conn_ptr) noexcept;

/// Observation hook for SERVER_W3ROUTE_PLAYERINFO.
/// @param conn_ptr  Opaque legacy `t_connection*` (w3route connection).
int pvpgn_v3_observe_w3route_playerinfo(void* conn_ptr) noexcept;

/// Observation hook for SERVER_W3ROUTE_LEVELINFO.
/// @param conn_ptr  Opaque legacy `t_connection*` (w3route connection).
int pvpgn_v3_observe_w3route_levelinfo(void* conn_ptr) noexcept;

/// Observation hook for SERVER_W3ROUTE_STARTGAME1.
/// @param conn_ptr  Opaque legacy `t_connection*` (w3route connection).
int pvpgn_v3_observe_w3route_startgame1(void* conn_ptr) noexcept;

/// Observation hook for SERVER_W3ROUTE_STARTGAME2.
/// @param conn_ptr  Opaque legacy `t_connection*` (w3route connection).
int pvpgn_v3_observe_w3route_startgame2(void* conn_ptr) noexcept;

}  // extern "C"
