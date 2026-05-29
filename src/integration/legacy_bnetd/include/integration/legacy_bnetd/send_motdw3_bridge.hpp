// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_motdw3_bridge.hpp
/// Strangler-fig hook for SERVER_MOTD_W3 (SID_MOTD, 0x1A).
///
/// `pvpgn_v3_send_motdw3` encodes a `MotdReply` via the v3 codec and ships
/// the bytes through the registered send_packet handler.
///
/// Wire layout:
///   msg_type (u8) + curr_time (u32 LE) + first_news_time (u32 LE) +
///   timestamp (u32 LE) + timestamp2 (u32 LE) + text (cstring)
///
/// Returns:
///   1  -> packet forwarded; legacy caller MUST skip its own emit path.
///   0  -> no handler, encoder failure, or handler declined; fall back.
///   -1 -> handler reported hard failure.

#ifdef __cplusplus
extern "C" {
#endif

/// Build a SERVER_MOTD_W3 (SID_MOTD, 0x1A) packet and push it to the
/// connection's out-queue via the registered send_packet handler.
///
/// Parameters:
///   - `conn_ptr`        : opaque legacy `t_connection*`.
///   - `msg_type`        : message type byte (1 = news entry).
///   - `curr_time`       : server-side wall clock (u32).
///   - `first_news_time` : oldest news item's timestamp (u32).
///   - `timestamp`       : this news item's timestamp (u32).
///   - `timestamp2`      : right-panel marker timestamp (u32).
///   - `text`            : NUL-terminated displayable message text.
int pvpgn_v3_send_motdw3(void*        conn_ptr,
                          unsigned int msg_type,
                          unsigned int curr_time,
                          unsigned int first_news_time,
                          unsigned int timestamp,
                          unsigned int timestamp2,
                          char const*  text) noexcept;

#ifdef __cplusplus
}
#endif
