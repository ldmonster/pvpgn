// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Observation-only strangler-fig bridge for the keepalive
// family: SID_PING (`_client_pingreq`) and SID_ECHO reply
// (`_client_echoreply`). Coalesced behind a single entry point
// keyed by `op`. Always returns 0; null conn = no log.

extern "C" int pvpgn_v3_keepalive_dispatch(void* conn_ptr,
                                               char const* op) noexcept;
