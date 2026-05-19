// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Observation-only strangler-fig bridge for SID_GAME_REPORT (0x40).
//
// Legacy `_client_gamereport` owns the full result-accounting and
// account-update state machine. This bridge is invoked at the entry
// point with the player_count parsed from the packet header and the
// username; it always returns 0 so legacy retains full control. The
// purpose at this slice is structured-log telemetry only, enabling
// later promotion to authoritative ownership once the surrounding
// state (game registry, account stats) has its own v3 facade.

extern "C" int pvpgn_v3_gamereport_try(void* conn_ptr,
                                       char const* username,
                                       unsigned int player_count) noexcept;
