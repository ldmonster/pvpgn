// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Observation-only strangler-fig bridge for the legacy
// `news_load(filename)` / `news_unload()` pair in
// `src/bnetd/news.cpp`. Mirrors anongame_infos_bridge: a v3-native
// news loader can later be swapped in behind this single observation
// point.
//
// Contract: always return 0 -- legacy MUST fall through and run the
// real loader / unloader.

extern "C" int pvpgn_v3_news_load_try(char const* filename) noexcept;
extern "C" int pvpgn_v3_news_unload_try(void) noexcept;
