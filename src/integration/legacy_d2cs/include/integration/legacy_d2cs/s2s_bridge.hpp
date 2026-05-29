// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file s2s_bridge.hpp
/// R233(2) -- observation-only strangler-fig bridge for the legacy
/// `s2s_init()` server-to-server bootstrap in `src/d2cs/s2s.cpp`.
/// `s2s_init` opens the outbound bnetd link used to relay realm
/// state; this bridge layers structured telemetry so a v3-native
/// realm-link service can later be swapped in behind a single flip.
///
/// Contract: always returns 0 -- legacy MUST fall through and run the
/// real `s2s_init` body.

extern "C" int pvpgn_v3_d2cs_s2s_init_try(void) noexcept;
