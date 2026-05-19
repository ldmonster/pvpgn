// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Observation-only strangler-fig bridge for the packet-sending
// functions in `src/bnetd/clan.cpp`.  A single coalesced entry
// point logs every clan send op with `{op}` so we can sample the
// call distribution before promoting any individual sender to v3.
// Always returns 0; null conn = no log.

extern "C" int pvpgn_v3_clan_send_try(void* conn_ptr,
                                      char const* op) noexcept;
