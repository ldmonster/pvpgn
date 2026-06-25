// SPDX-License-Identifier: GPL-2.0-or-later
//
// Loader for the `[ICON_REQUIRED_*]` blocks in `anongame_infos.conf`.
//
// The legacy file (see `conf/anongame_infos.conf.in` ~line 245)
// declares three independent tables:
//
//   [ICON_REQUIRED_RACE_WINS_WAR3]   -> 4 levels (Level1..Level4)
//   [ICON_REQUIRED_RACE_WINS_W3XP]   -> 5 levels (Level1..Level5)
//   [ICON_REQUIRED_TOURNEY_WINS]     -> 5 levels (Level1..Level5)
//
// These thresholds are the win counts a player must reach to unlock
// each level of race-icon (or tournament icon). The legacy lookups
// `anongame_infos_get_ICON_REQ(level, clienttag)` and
// `anongame_infos_get_ICON_REQ_TOURNEY(level)` (in
// `bnetd/anongame_infos.cpp`) read directly out of these tables.
//
// This loader returns a typed, copyable `IconReqTable` so the
// icon-table service can run pure (no global state, no file IO).

#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include "application/icon_table/icon_table.hpp"
#include "core/error.hpp"
#include "core/result.hpp"

namespace pvpgn::infra::legacy_config {

using IconReqTable = pvpgn::application::icon_table::IconReqTable;
using pvpgn::application::icon_table::kIconReqWar3Levels;
using pvpgn::application::icon_table::kIconReqW3xpLevels;
using pvpgn::application::icon_table::kIconReqTourneyLevels;

/// Parse `[ICON_REQUIRED_*]` blocks from `anongame_infos.conf`.
///
/// The returned table starts from the legacy built-in defaults
/// (`kIconReq*Defaults`, mirroring `anongame_infos_ICON_REQ_init`);
/// only levels actually present in the file override them. So a
/// missing block or missing level keeps its protective default
/// threshold rather than collapsing to zero (which would unlock every
/// icon for every user and defeat the icon-switch-hack protection).
///
/// Returns `NotFound` if the file cannot be opened, `InvalidArgument`
/// for malformed `LevelN = ...` lines. Callers that treat `NotFound`
/// as "no config" should fall back to a default-constructed
/// `IconReqTable`, which already carries the built-in defaults.
core::Result<IconReqTable> load_icon_req_table(std::string_view path);

}  // namespace pvpgn::infra::legacy_config
