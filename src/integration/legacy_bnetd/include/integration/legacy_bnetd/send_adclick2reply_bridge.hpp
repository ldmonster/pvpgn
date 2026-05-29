// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef PVPGN_V3_INTEGRATION_LEGACY_BNETD_SEND_ADCLICK2REPLY_BRIDGE_HPP
#define PVPGN_V3_INTEGRATION_LEGACY_BNETD_SEND_ADCLICK2REPLY_BRIDGE_HPP

#ifdef __cplusplus
extern "C" {
#endif

// Strangler-fig bridge for SERVER_ADCLICKREPLY2 (SID_ADCLICK2 = 0x41).
//
// Wire layout: header(4) + adid(4) + link(cstring).
//
// `link` must be a non-null NUL-terminated cstring.
//
// Returns 1 on success, 0 on decline (no handler / null conn /
// oversize), -1 on hard transport failure.
int pvpgn_v3_send_adclick2reply(void*       conn_ptr,
                                 unsigned int adid,
                                 char const*  link) noexcept;

#ifdef __cplusplus
}
#endif

#endif
