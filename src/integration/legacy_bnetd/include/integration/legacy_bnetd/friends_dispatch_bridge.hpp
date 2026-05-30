// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Observation-only strangler-fig bridge for SID_FRIENDSLISTREQ
// and SID_FRIENDINFOREQ. Coalesced behind a single entry point
// keyed by an `op` tag. Always returns 0; null conn = no log.

extern "C" int pvpgn_v3_friends_dispatch(void* conn_ptr,
                                             char const* op) noexcept;
