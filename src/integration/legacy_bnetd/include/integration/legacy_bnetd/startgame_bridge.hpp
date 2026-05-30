// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Observation-only strangler-fig bridge for SID_STARTGAME variants:
//   * STARTGAME1 (gateway protocol version 1, SC/D2 era)
//   * STARTGAME3 (gateway protocol version 3, SC expansion era)
//   * STARTGAME4 (gateway protocol version 4, W3 era)
//
// Legacy `_client_startgame{1,3,4}` retains full ownership of the
// game registry, ack reply, ladder authority check, and W3 echo
// kickoff. This bridge is invoked after the legacy handler has
// finished parsing the inbound strings; it never alters state and
// always returns 0. Purpose: structured-log telemetry for the
// game-creation surface so later v3 slices can be promoted with a
// behaviour reference.

extern "C" int pvpgn_v3_startgame(void* conn_ptr,
                                      unsigned int version,
                                      char const* gamename,
                                      char const* gameinfo,
                                      unsigned int bngtype,
                                      unsigned int status,
                                      unsigned int flag,
                                      unsigned int option) noexcept;
