// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Observation-only strangler-fig bridges for the game-discovery /
// join surface:
//   * SID_GAMELISTREQ (0x09) -- client polls for game list or a
//     specific game by name (Battle.net "Join" view).
//   * SID_JOIN_GAME   (0x22) -- client attempts to join a known
//     game by name + password.
//
// Legacy `_client_gamelistreq` / `_client_joingame` retain full
// ownership of the game registry traversal, password check, ladder
// authority gate, channel-part side-effect, and SERVER_GAMELISTREPLY
// composition. These bridges run before legacy mutates state and
// always return 0 -- structured-log telemetry only at this slice.

extern "C" int pvpgn_v3_gamelistreq_try(void* conn_ptr,
                                        char const* gamename,
                                        unsigned int bngtype) noexcept;

extern "C" int pvpgn_v3_joingame_try(void* conn_ptr,
                                     char const* gamename) noexcept;
