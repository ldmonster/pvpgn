// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Observation-only strangler-fig bridge for the legacy
// `runprog_open(command) / runprog_close(pp)` subprocess wrapper in
// `src/bnetd/runprog.cpp` (used by the legacy ad-banner picker and
// ad-rotation rebuild paths). The legacy implementation owns the
// fork()+pipe() plumbing; this bridge only layers structured
// telemetry so a v3-native runner can later be swapped in behind a
// single flip.
//
// Contract: always return 0 -- legacy MUST fall through and run the
// real fork()+exec().

extern "C" int pvpgn_v3_runprog_open_try(char const* command) noexcept;
extern "C" int pvpgn_v3_runprog_close_try(void) noexcept;
