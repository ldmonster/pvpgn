// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef PVPGN_V3_INTEGRATION_LEGACY_BNETD_SEND_CHARLISTREPLY_BRIDGE_HPP
#define PVPGN_V3_INTEGRATION_LEGACY_BNETD_SEND_CHARLISTREPLY_BRIDGE_HPP

#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

// Strangler-fig bridge for SERVER_CHARLISTREPLY (SID_CHARLIST = 0x19 / 0x37).
//
// Wire layout: header(4) + unknown1(4) + max_chars(4) + count(4) +
// raw character-data bytes (variable).
//
// `char_data` may be nullptr when `char_data_len` is 0 (empty list).
// `unknown1` and `max_chars` are the fixed fields from the legacy
// SERVER_UNKNOWN_37 packet header.
//
// Returns 1 on success, 0 on decline (no handler / null conn /
// oversize), -1 on hard transport failure.
int pvpgn_v3_send_charlistreply(void*                conn_ptr,
                                 unsigned int         unknown1,
                                 unsigned int         max_chars,
                                 unsigned int         count,
                                 unsigned char const* char_data,
                                 unsigned int         char_data_len) noexcept;

#ifdef __cplusplus
}
#endif

#endif
