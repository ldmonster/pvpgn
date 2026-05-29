// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file anongame_lobby_bridge.hpp
/// Strangler-fig dispatcher for the BNCS anonymous-game lobby
/// admit flow (CLIENT_FINDANONGAME enqueue / CLIENT_ANONGAME_SEARCH).
///
/// Stage 1 (this round): base half only -- C ABI + atomic handler
/// storage + idempotent install hook. No legacy adapter is wired
/// yet, so without an installed handler the entry returns -1
/// (caller falls back to legacy `_anongame_queue`). Stage 2 (next
/// round) will add the linked adapter over the legacy
/// `anongame_queue` global and wire `_client_findanongame` /
/// `_client_anongame_search` through this entry point.

#ifdef __cplusplus
extern "C" {
#endif

/// Admit the given connection's account into the anonymous-game
/// lobby for `game_type`. The handler decides via the application
/// `dispatch_admit` whether to queue, promote a full bracket, treat
/// as duplicate, or reject. On promotion the handler is responsible
/// for spawning the game and notifying every entrant via the
/// existing send_anongame bridges.
///
/// Returns:
///   1  -> handler ran and the entrant was admitted (queued,
///         promoted, or duplicate-suppressed). Caller MUST skip
///         its own enqueue path.
///   0  -> handler installed but the admit could not be processed
///         (encoder / send-packet failure). Caller MUST NOT retry
///         legacy -- the connection is already in a bad state.
///   -1 -> no handler installed. Caller falls back to legacy
///         `_anongame_queue`.
int pvpgn_v3_anongame_lobby_apply(void* conn_ptr, unsigned game_type) noexcept;

#ifdef __cplusplus
}
#endif

namespace pvpgn::integration::legacy_bnetd {

/// Handler signature: same args as the extern C entry. Returns 1
/// on success, 0 on send / encode failure.
using AnonGameLobbyHandler = int (*)(void* conn_ptr, unsigned game_type);

void                  set_anongame_lobby_handler(AnonGameLobbyHandler h) noexcept;
AnonGameLobbyHandler  get_anongame_lobby_handler() noexcept;

/// Idempotent install hook -- a no-op in stage 1 (no legacy
/// adapter exists yet). Reserved for stage 2 wiring.
void install_legacy_anongame_lobby_handler();

}  // namespace pvpgn::integration::legacy_bnetd
