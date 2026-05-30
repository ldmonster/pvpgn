// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Observation-only strangler-fig bridge for the SID_CLAN_*
// handler family in `handle_bnet.cpp`. A single coalesced
// entry point logs every clan op with `{op}` so we can sample
// the call distribution before promoting any individual handler
// to v3. Always returns 0; null conn = no log.

extern "C" int pvpgn_v3_clan_dispatch(void* conn_ptr,
                                          char const* op) noexcept;
