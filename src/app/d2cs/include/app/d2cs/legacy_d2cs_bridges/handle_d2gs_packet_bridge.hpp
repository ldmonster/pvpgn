// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file handle_d2gs_packet_bridge.hpp
/// Observation-only bridge for the legacy
/// `handle_d2gs_packet()` dispatcher in `src/d2cs/handle_d2gs.cpp`.
/// Fires once per inbound packet from a registered d2gs gameserver.
///
/// Parameters are scalars only (see handle_d2cs_packet_bridge.hpp
/// for the full contract).

extern "C" int pvpgn_v3_d2cs_handle_d2gs_packet(
    int sd,
    unsigned int packet_type,
    unsigned int packet_size) noexcept;
