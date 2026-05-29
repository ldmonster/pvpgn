// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Observation-only strangler-fig bridge for the two anongame entry
// points routed from outside `handle_anongame_packet`:
//   * `handle_anongame_search` -- called from
//     `handle_anongame_packet` for sub-options SEARCH /
//     AT_SEARCH / AT_INVITER_SEARCH. The legacy handler lives in
//     `anongame.cpp`, not `handle_anongame.cpp`.
//   * `handle_anongame_join`   -- called from
//     `_client_joingame` (handle_bnet.cpp) when the joined game
//     name is the magic string "BNet".
//
// Always returns 0 so legacy retains full ownership of the search
// queues, matchmaking, and W3 route push.

extern "C" int pvpgn_v3_anongame_entry_try(void* conn_ptr,
                                           char const* kind) noexcept;
