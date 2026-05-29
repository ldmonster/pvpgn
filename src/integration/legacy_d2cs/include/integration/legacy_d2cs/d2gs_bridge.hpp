// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file d2gs_bridge.hpp
/// R236(3) -- observation-only strangler-fig bridges for the legacy
/// d2gs-list lifecycle in `src/d2cs/d2gs.cpp`:
///
///   * `d2gslist_create()`             -- one-shot bootstrap at start.
///   * `d2gslist_reload(gslist)`       -- reload on SIGHUP. The
///                                        `gslist` argument is a
///                                        C-string that points to a
///                                        comma-separated server
///                                        list; the bridge MUST treat
///                                        a null pointer as legal.
///   * `d2gslist_destroy()`            -- shutdown teardown.
///
/// Contract: always return 0 -- legacy MUST fall through and run the
/// real body.

extern "C" int pvpgn_v3_d2cs_d2gslist_create_try(void)  noexcept;
extern "C" int pvpgn_v3_d2cs_d2gslist_destroy_try(void) noexcept;
extern "C" int pvpgn_v3_d2cs_d2gslist_reload_try(const char* gslist) noexcept;
