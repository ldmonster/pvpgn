// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef PVPGN_V3_INTEGRATION_LEGACY_BNETD_SEND_ADREPLY_BRIDGE_HPP
#define PVPGN_V3_INTEGRATION_LEGACY_BNETD_SEND_ADREPLY_BRIDGE_HPP

#ifdef __cplusplus
extern "C" {
#endif

// Strangler-fig bridge for SERVER_ADREPLY (SID_CHECKAD = 0x21).
//
// Wire layout: header(4) + adid(4) + extension_tag(4) + timestamp(8) +
// filename(cstring) + link(cstring).
//
// `filename` and `link` must be non-null NUL-terminated cstrings.
//
// Returns 1 on success, 0 on decline (no handler / null conn /
// oversize), -1 on hard transport failure.
int pvpgn_v3_send_adreply(void*                conn_ptr,
                           unsigned int         adid,
                           unsigned int         extension_tag,
                           unsigned long long   timestamp,
                           char const*          filename,
                           char const*          link);

#ifdef __cplusplus
}
#endif

#endif
