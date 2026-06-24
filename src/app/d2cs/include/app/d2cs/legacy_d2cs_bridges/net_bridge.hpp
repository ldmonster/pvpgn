// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file net_bridge.hpp
/// Observation-only bridges for the legacy
/// socket helpers in `src/d2cs/net.cpp`. Three entry points are
/// covered:
///   * `net_socket(type)`            -- low-level non-blocking socket
///                                      creation for outbound (s2s)
///                                      connections.
///   * `net_check_connected(sock)`   -- per-readiness SO_ERROR probe
///                                      on a pending outbound socket.
///   * `net_listen(ip, port, type)`  -- startup-only listener creation
///                                      for inbound traffic.
///
/// All bridges take POD scalars only; legacy `psock_*` state never
/// crosses the seam. Contract: always returns 0, legacy MUST fall
/// through and run the real syscall sequence.

extern "C" int pvpgn_v3_d2cs_net_socket(int type) noexcept;

extern "C" int pvpgn_v3_d2cs_net_check_connected(int sock) noexcept;

extern "C" int pvpgn_v3_d2cs_net_listen(
    unsigned int ip,
    unsigned int port,
    int type) noexcept;
