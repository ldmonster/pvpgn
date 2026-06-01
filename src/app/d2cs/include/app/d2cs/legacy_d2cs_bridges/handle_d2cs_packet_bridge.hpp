// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file handle_d2cs_packet_bridge.hpp
/// R235(1) -- observation-only strangler-fig bridge for the legacy
/// `d2cs_handle_d2cs_packet()` dispatcher in
/// `src/d2cs/handle_d2cs.cpp`. Fires once per inbound client packet
/// (after the init handshake has placed the connection in the
/// d2cs-client class).
///
/// Parameters are scalars only -- the bridge never inspects the
/// legacy `t_connection` or `t_packet`:
///   * `sd`          -- raw socket descriptor of the originating
///                      connection.
///   * `packet_type` -- result of `packet_get_type(packet)`.
///   * `packet_size` -- result of `packet_get_size(packet)`.
///
/// Contract: always returns 0 -- legacy MUST fall through and run the
/// real dispatch.

extern "C" int pvpgn_v3_d2cs_handle_d2cs_packet(
    int sd,
    unsigned int packet_type,
    unsigned int packet_size) noexcept;
