// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file server_bridge.hpp
/// R234(1) -- observation-only strangler-fig bridge for the legacy
/// `d2cs_server_process()` event-loop entry in `src/d2cs/server.cpp`.
/// `d2cs_server_process` is the d2cs main loop bootstrap: once the
/// process has finished startup it enters this function and runs
/// until shutdown. The bridge fires exactly once, immediately on
/// entry, so a v3-native event loop can later be swapped in behind a
/// single flip.
///
/// Contract: always returns 0 -- legacy MUST fall through and run the
/// real `d2cs_server_process` body.

extern "C" int pvpgn_v3_d2cs_server_process(void) noexcept;
