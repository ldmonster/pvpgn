// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Observation-only bridge for the `anongame_infos.cpp` runtime
// data getters. The legacy module exposes ~12 small `extern`
// functions that look up icons / URLs / descriptions / map blobs
// by queue, level or (clienttag, versionid). Instead of wiring
// one bridge per getter we provide a single coalesced
// observation entry point keyed by a free-form `kind` tag plus
// up to three string slots.
//
// All slots accept `nullptr` and are normalised to the empty
// string in the log fields. Always returns 0; the legacy code
// continues to own the lookup result.

extern "C" int pvpgn_v3_anongame_infos_get_try(char const* kind,
                                               char const* arg0,
                                               char const* arg1,
                                               char const* arg2) noexcept;
