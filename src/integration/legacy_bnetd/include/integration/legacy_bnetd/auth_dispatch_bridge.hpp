// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Observation-only strangler-fig bridge for SID_AUTH_INFO and
// the SID_AUTH_CHECK family (`SID_AUTH_*` = `authreq1` +
// `authreq109` in the legacy code). Coalesced behind a single
// entry point keyed by `op`. Always returns 0; null conn = no
// log.

extern "C" int pvpgn_v3_auth_dispatch(void* conn_ptr,
                                          char const* op) noexcept;
