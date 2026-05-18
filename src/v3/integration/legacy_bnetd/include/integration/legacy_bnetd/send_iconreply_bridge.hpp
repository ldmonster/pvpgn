// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef PVPGN_V3_INTEGRATION_LEGACY_BNETD_SEND_ICONREPLY_BRIDGE_HPP
#define PVPGN_V3_INTEGRATION_LEGACY_BNETD_SEND_ICONREPLY_BRIDGE_HPP

#ifdef __cplusplus
extern "C" {
#endif

// Strangler-fig bridge for SERVER_ICONREPLY (0x2d).
//
// Wire layout: header(4) + u64 timestamp (LE) + NUL-terminated filename.
// `filename` may be nullptr -> emits a single NUL terminator (empty
// string), matching legacy `packet_append_string(rpacket, "")` behavior.
//
// Returns 1 on success, 0 on decline, -1 on hard transport failure.
int pvpgn_v3_send_iconreply(void* conn_ptr,
                            unsigned long long timestamp,
                            char const* filename);

#ifdef __cplusplus
}
#endif

#endif
