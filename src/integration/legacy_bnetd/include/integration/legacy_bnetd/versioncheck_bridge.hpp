// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Observation-only strangler-fig bridge for the legacy
// `load_versioncheck_conf(filename)` /
// `unload_versioncheck_conf()` pair in `src/bnetd/versioncheck.cpp`
// (game-client version-check table loader / unloader).
//
// Contract: always return 0 -- legacy MUST fall through and run the
// real loader / unloader.

extern "C" int pvpgn_v3_versioncheck_load_try(char const* filename) noexcept;
extern "C" int pvpgn_v3_versioncheck_unload_try(void) noexcept;
