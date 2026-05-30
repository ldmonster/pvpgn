// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef PVPGN_V3_INTEGRATION_LEGACY_BNETD_CHANNEL_STATE_BRIDGE_HPP
#define PVPGN_V3_INTEGRATION_LEGACY_BNETD_CHANNEL_STATE_BRIDGE_HPP

#ifdef __cplusplus
extern "C" {
#endif

// Observation bridges for SID_JOINCHANNEL (0x0c) and SID_LEAVECHANNEL
// (0x10) handlers. These packets are pure side-effect on the server
// (no direct reply), the user-visible state change is emitted via
// SID_CHATEVENT JOIN / LEAVE which is already covered by the
// `send_chatevent_compose_bridge`. The bridges here exist so the v3
// observability layer (structured logging, future telemetry,
// classification-driven routing) can see every channel-state intent
// from the legacy stack without altering reply behaviour.
//
// `channel_name` must be a NUL-terminated cstring (legacy
// `packet_get_str_const` guarantees a terminator within
// `MAX_CHANNELNAME_LEN`). `flag` encodes the JOINCHANNEL subkind:
//   0 = NORMAL
//   1 = GENERIC
//   2 = CREATE
// matching `t_client_joinchannel_flags` in
// `src/common/bnet_protocol.h`.
//
// Returns 0 -- both bridges are observation-only; the legacy state
// machine always continues. A non-zero return is reserved for a
// future opt-in v3 take-over of the join logic.
int pvpgn_v3_joinchannel(void* conn_ptr,
                             char const* channel_name,
                             unsigned int flag);

int pvpgn_v3_leavechannel(void* conn_ptr);

#ifdef __cplusplus
}
#endif

#endif
