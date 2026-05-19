// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef PVPGN_V3_INTEGRATION_LEGACY_BNETD_SEND_PLAYERINFOREPLY_BRIDGE_HPP
#define PVPGN_V3_INTEGRATION_LEGACY_BNETD_SEND_PLAYERINFOREPLY_BRIDGE_HPP

#ifdef __cplusplus
extern "C" {
#endif

// Strangler-fig bridge for SERVER_PLAYERINFOREPLY (SID_USERDATA = 0x0A).
//
// Wire layout: header(4) + account_name(cstring) + player_info(cstring) +
// username(cstring).
//
// All three string parameters must be non-null NUL-terminated cstrings.
// Pass empty strings ("") for absent values.
//
// Returns 1 on success, 0 on decline (no handler / null conn /
// oversize), -1 on hard transport failure.
int pvpgn_v3_send_playerinforeply(void*       conn_ptr,
                                   char const* account_name,
                                   char const* player_info,
                                   char const* username);

#ifdef __cplusplus
}
#endif

#endif
