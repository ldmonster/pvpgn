// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Observation-only strangler-fig bridge for the anongame_infos
// lifecycle. The legacy `anongame_infos_load(filename)` / 
// `anongame_infos_unload()` own parsing of `anongame_infos.conf`
// and the in-memory list `anongame_infos`. The v3 infra already
// has a parallel facade
// (`src/v3/infra/legacy_config/src/anongame_infos_loader.cpp`)
// covered by `anongame_infos_loader_test`; this bridge layers
// telemetry on top of the legacy load/unload calls so future
// promotion can flip the loader behind a single switch.

extern "C" int pvpgn_v3_anongame_infos_load_try(char const* filename) noexcept;
extern "C" int pvpgn_v3_anongame_infos_unload_try(void) noexcept;
