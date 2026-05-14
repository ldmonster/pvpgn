// SPDX-License-Identifier: GPL-2.0-or-later
//
// Legacy bridge: parse `anongame_infos.conf` into a typed
// `application::anongame_infoply::AnonGameInfoSnapshot`.
//
// The legacy file format is a custom INI-like dialect:
//
//   [URL]
//   server_URL  = "http://..."
//   player_URL  = "http://...?user="     # inline comment OK
//   tourney_URL = "..."
//   clan_URL    = "..."
//   ladder_PG_1v1_URL  = "..."
//   ...
//
//   [DEFAULT_DESC]
//   gametype_1v1_short = "1v1"
//   gametype_1v1_long  = "One vs. One"
//   ...
//
//   [deDE]      # language-specific DESC block
//   gametype_1v1_short = "Eins gegen Eins"
//   ...
//
// Reference: `src/bnetd/anongame_infos.cpp` (`anongame_infos_load`).
//
// This first cut handles the `[URL]` section (-> AnonGameUrlPayload),
// the `[DEFAULT_DESC]` section's `gametype_*_short`/`gametype_*_long`
// pairs (-> AnonGameDescPayload, section_id = 0 / PG), and the 10
// `ladder_*_URL` / `ladder_*_desc` keys (-> AnonGameLadrPayload). The
// TYPE / MAP loaders and per-locale DESC variants will be added in
// follow-up batches because their layouts depend on data outside this
// file (legacy `anongame_maplists.conf`, the AT/TY section enums, etc).

#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include "core/error.hpp"
#include "core/result.hpp"

#include "application/anongame_infoply/inforeply_builder.hpp"

namespace pvpgn::infra::legacy_config {

/// Result of parsing a multi-locale `anongame_infos.conf`.
///
/// `default_snapshot` is built from the `[DEFAULT_DESC]` section
/// (plus the locale-independent `[URL]` keys). `by_lang` carries one
/// snapshot per `[<langID>]` section encountered (e.g. `"deDE"`,
/// `"ruRU"`, `"zhCN"`); each one shares the same URL/LADR layout as
/// the default but reuses the locale-specific gametype short/long
/// strings (and locale-specific ladder descs) where present, falling
/// back to the default block when missing — mirroring the legacy
/// `anongame_infos_DESC_get_DESC(langID, ...)` lookup.
struct MultilocaleSnapshotSet {
    application::anongame_infoply::AnonGameInfoSnapshot default_snapshot;
    std::unordered_map<std::string,
                       application::anongame_infoply::AnonGameInfoSnapshot>
        by_lang;
};

/// Read the legacy `anongame_infos.conf` from `path` and return a
/// single snapshot built from the `[DEFAULT_DESC]` block (equivalent
/// to `load_anongame_infos_multilocale(path)->default_snapshot`).
/// See the multi-locale function below for full coverage.
core::Result<application::anongame_infoply::AnonGameInfoSnapshot>
load_anongame_infos(std::string_view path);

/// Read the legacy `anongame_infos.conf` from `path` and return one
/// snapshot per locale plus a `default_snapshot`. Locale snapshots
/// inherit the default's URL/LADR layout and any missing
/// gametype/ladder description strings.
core::Result<MultilocaleSnapshotSet>
load_anongame_infos_multilocale(std::string_view path);

}  // namespace pvpgn::infra::legacy_config
