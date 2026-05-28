// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file handle_bnetd_packet_bridge.hpp
/// R235(3) -- observation-only strangler-fig bridge for the legacy
/// `handle_bnetd_packet()` dispatcher in
/// `src/d2cs/handle_bnetd.cpp`. Fires once per inbound packet from
/// the upstream bnetd realm-link (s2s) connection.
///
/// Parameters are scalars only (see handle_d2cs_packet_bridge.hpp
/// for the full contract).

extern "C" int pvpgn_v3_d2cs_handle_bnetd_packet_try(
    int sd,
    unsigned int packet_type,
    unsigned int packet_size) noexcept;
