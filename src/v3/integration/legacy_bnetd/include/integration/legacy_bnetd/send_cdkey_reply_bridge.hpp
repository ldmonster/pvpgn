// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef PVPGN_V3_INTEGRATION_LEGACY_BNETD_SEND_CDKEY_REPLY_BRIDGE_HPP
#define PVPGN_V3_INTEGRATION_LEGACY_BNETD_SEND_CDKEY_REPLY_BRIDGE_HPP

#ifdef __cplusplus
extern "C" {
#endif

// Strangler-fig bridge for SERVER_CDKEYREPLY (0x30).
//
// Wire layout: header(4) + u32 message (LE) + NUL-terminated owner string.
// `owner` may be nullptr -> emits a single NUL terminator (empty string).
//
// Returns 1 on success, 0 on decline, -1 on hard transport failure.
int pvpgn_v3_send_cdkeyreply(void* conn_ptr,
                              unsigned int message,
                              char const* owner) noexcept;

// Strangler-fig bridge for SERVER_CDKEYREPLY2 (0x36).
//
// Wire layout: header(4) + u32 result (LE) + NUL-terminated owner string.
// `owner` may be nullptr -> emits a single NUL terminator (empty string).
//
// Returns 1 on success, 0 on decline, -1 on hard transport failure.
int pvpgn_v3_send_cdkeyreply2(void* conn_ptr,
                               unsigned int result,
                               char const* owner) noexcept;

// Strangler-fig bridge for SERVER_CDKEYREPLY3 (0x42).
//
// Wire layout: header(4) + u32 message (LE) + NUL-terminated owner_name.
// `owner_name` may be nullptr -> emits a single NUL terminator (empty string).
//
// Returns 1 on success, 0 on decline, -1 on hard transport failure.
int pvpgn_v3_send_cdkeyreply3(void* conn_ptr,
                               unsigned int message,
                               char const* owner_name) noexcept;

#ifdef __cplusplus
}
#endif

#endif
