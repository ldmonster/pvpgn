// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file handle_signal_bridge.hpp
/// R232(3) -- observation-only strangler-fig bridges for the legacy
/// signal-handling lifecycle in `src/d2dbs/handle_signal.cpp`:
///
///   * `d2dbs_handle_signal_init()` -- one-shot signal-handler
///     installation (POSIX only; the WIN32 build uses a different
///     console-control-handler path).
///   * `d2dbs_handle_signal()` -- per-tick signal dispatch invoked
///     from the dbs_server_loop().
///
/// Contract: always return 0 -- legacy MUST fall through and run the
/// real body. The bridges MUST NOT install or unregister any signal
/// handlers themselves.

extern "C" int pvpgn_v3_d2dbs_handle_signal_init_try(void) noexcept;

extern "C" int pvpgn_v3_d2dbs_handle_signal_try(void) noexcept;
