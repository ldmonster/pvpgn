// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef PVPGN_V3_INTEGRATION_LEGACY_BNETD_SEND_CHANNELLIST_BRIDGE_HPP
#define PVPGN_V3_INTEGRATION_LEGACY_BNETD_SEND_CHANNELLIST_BRIDGE_HPP

#ifdef __cplusplus
extern "C" {
#endif

// Strangler-fig bridge for SERVER_CHANNELLIST (0x0B).
//
// Wire layout: header(4) + zero-or-more NUL-terminated channel names +
// a single trailing NUL (empty cstring terminator). Matches the
// legacy `_client_progident2` reply built with `packet_append_string`
// per channel and a final `packet_append_string(rpacket, "")`.
//
// `names` may be nullptr when `count` is 0; otherwise it must point to
// an array of `count` non-null, NUL-terminated cstrings. Empty entries
// are skipped (the v3 encoder rejects empties; we mirror legacy which
// only ever appends non-empty channel names).
//
// Returns 1 on success, 0 on decline (no handler / null conn /
// oversize), -1 on hard transport failure.
int pvpgn_v3_send_channellist(void* conn_ptr,
                              char const* const* names,
                              unsigned int count);

#ifdef __cplusplus
}
#endif

#endif
