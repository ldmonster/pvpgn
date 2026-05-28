// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file game_bridge.hpp
/// R238 -- observation-only strangler-fig bridges for the legacy
/// `d2cs_gamelist_create()` / `d2cs_gamelist_destroy()` /
/// `d2cs_game_create()` / `game_destroy()` entry points in
/// `src/d2cs/game.cpp`. The bridges fire AFTER the legacy state
/// mutation is committed (post `list_prepend_data` for create,
/// pre `list_remove_data` for destroy -- so v3 sees every attempt
/// matched by the eventlog line that legacy emits).
///
/// All symbols take POD scalars and null-safe `const char*` strings
/// only -- the bridge never inspects the legacy `t_game` struct.
/// Caller (game.cpp) holds the legacy gamelist lock implicitly via
/// the single-threaded I/O loop, so string buffers cannot mutate
/// for the duration of the bridge call.
///
/// Contract: all bridges return 0; legacy code MUST fall through.

extern "C" int pvpgn_v3_d2cs_gamelist_create_try(void) noexcept;

extern "C" int pvpgn_v3_d2cs_gamelist_destroy_try(void) noexcept;

/// Fired after a successful `d2cs_game_create()`:
///   * `id`       -- the newly-assigned `game->id` (always non-zero).
///   * `gamename` -- the legacy `gamename` parameter (NUL-terminated;
///                   may be `nullptr` only as a defensive guard).
///   * `gameflag` -- the raw `gameflag` argument (host-byte-order
///                   uint32, decimal-rendered).
extern "C" int pvpgn_v3_d2cs_game_create_try(
    unsigned int id,
    const char* gamename,
    unsigned int gameflag) noexcept;

/// Fired at the top of `game_destroy()` BEFORE the list mutation:
///   * `id`       -- `game->id`.
///   * `gamename` -- `game->name` (legacy guarantees non-null but the
///                   bridge handles null defensively).
extern "C" int pvpgn_v3_d2cs_game_destroy_try(
    unsigned int id,
    const char* gamename) noexcept;

/// R239(1) -- observation bridge for `game_set_d2gs_gameid()` --
/// links the d2cs-side game id to the upstream d2gs's local id.
extern "C" int pvpgn_v3_d2cs_game_set_d2gs_gameid_try(
    unsigned int game_id,
    unsigned int d2gs_gameid) noexcept;

/// R239(2) -- observation bridge for `game_set_d2gs()` -- binds the
/// d2cs-side game to a specific d2gs gameserver. `d2gs_id` is 0 if
/// the caller passed a null `t_d2gs*` (legacy supports detaching).
extern "C" int pvpgn_v3_d2cs_game_set_d2gs_try(
    unsigned int game_id,
    unsigned int d2gs_id) noexcept;

/// R239(3) -- observation bridge for `game_set_created()` -- flips
/// the per-game "ack'd by d2gs" flag (0 -> 1 means the d2gs
/// confirmed the game; 1 -> 0 is the explicit reset path).
extern "C" int pvpgn_v3_d2cs_game_set_created_try(
    unsigned int game_id,
    unsigned int created) noexcept;

/// R240(1) -- observation bridge for `game_add_character()` -- fires
/// BEFORE the legacy update-or-insert decision. Both code paths
/// (in-place update of an existing charinfo, append of a brand-new
/// one) are observed identically; v3 can correlate by name.
extern "C" int pvpgn_v3_d2cs_game_add_character_try(
    unsigned int game_id,
    const char* charname,
    unsigned int chclass,
    unsigned int level) noexcept;

/// R240(2) -- observation bridge for `game_del_character()` -- fires
/// BEFORE the find/remove so v3 sees every attempt, including ones
/// that subsequently fail with "character not found".
extern "C" int pvpgn_v3_d2cs_game_del_character_try(
    unsigned int game_id,
    const char* charname) noexcept;
