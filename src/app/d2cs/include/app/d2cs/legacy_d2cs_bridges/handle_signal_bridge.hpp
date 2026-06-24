// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file handle_signal_bridge.hpp
/// Observation-only bridges for the legacy
/// signal-handling lifecycle in `src/d2cs/handle_signal.cpp`:
///
///   * `handle_signal_init()` -- one-shot signal-handler installation
///     (POSIX only; the WIN32 build uses console-control wrappers).
///   * `handle_signal()` -- per-tick signal dispatch invoked from
///     the d2cs server loop.
///
/// Contract: always return 0 -- legacy MUST fall through and run the
/// real body.

extern "C" int pvpgn_v3_d2cs_handle_signal_init(void) noexcept;

extern "C" int pvpgn_v3_d2cs_handle_signal(void) noexcept;
