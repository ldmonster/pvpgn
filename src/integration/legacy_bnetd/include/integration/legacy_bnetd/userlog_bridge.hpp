// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Observation-only strangler-fig bridge for the legacy
// `userlog_init()` / `userlog_append(account, text)` pair in
// `src/bnetd/userlog.cpp` (admin / operator command audit log).
// Surfaces what the legacy audit log saw so a v3-native audit
// sink can later be swapped in behind a single flip.
//
// Contract: always return 0 -- legacy MUST fall through and run the
// real init / append.

extern "C" int pvpgn_v3_userlog_init_try(void) noexcept;
extern "C" int pvpgn_v3_userlog_append_try(char const* username,
                                            char const* text) noexcept;
