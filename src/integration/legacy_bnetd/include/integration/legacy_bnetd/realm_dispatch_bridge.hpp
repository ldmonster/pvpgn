// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Observation-only strangler-fig bridge for the realm dispatch
// family: SID_QUERYREALMS (`_client_realmlistreq`),
// SID_QUERYREALMS2 (`_client_realmlistreq110`) and
// SID_LOGONREALMEX (`_client_realmjoinreq109`). Coalesced
// behind a single entry point keyed by `op`. Always returns 0;
// null conn = no log.

extern "C" int pvpgn_v3_realm_dispatch(void* conn_ptr,
                                           char const* op) noexcept;
