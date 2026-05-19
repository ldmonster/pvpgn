// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Observation-only strangler-fig bridge for the profile / stats
// dispatch family: SID_READUSERDATA (`_client_profilereq`),
// SID_GETCHANNELLIST stats variant... no:
// `_client_statsreq` (read user stats) and SID_WRITEUSERDATA
// (`_client_statsupdate`). Coalesced behind a single entry
// point keyed by `op`. Always returns 0; null conn = no log.

extern "C" int pvpgn_v3_profile_dispatch_try(void* conn_ptr,
                                             char const* op) noexcept;
