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
/// Missing blocks or missing levels are left at zero. Returns
/// `NotFound` if the file cannot be opened, `InvalidArgument` for
/// malformed `LevelN = ...` lines.
core::Result<IconReqTable> load_icon_req_table(std::string_view path);

}  // namespace pvpgn::infra::legacy_config
