// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Observation-only strangler-fig bridge for SID_FINDANONGAME (0x44)
// dispatch. The legacy `handle_anongame_packet` routes the inbound
// packet by its sub-option byte to one of nine handlers (profile,
// cancel, search/AT-search/AT-inviter-search, get-icon, set-icon,
// infos, tournament, profile-clan).
//
// This bridge runs at the dispatch entry and structured-logs the
// sub-option byte for telemetry. It does not interpret the rest of
// the packet and never alters legacy behaviour -- it always
// returns 0.

extern "C" int pvpgn_v3_anongame_dispatch_try(void* conn_ptr,
                                              unsigned int option) noexcept;
