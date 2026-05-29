// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_friendmove_ack_bridge.hpp
/// Strangler-fig hook for SERVER_FRIENDMOVE_ACK (SID 0x69).
/// Wire: 4-byte bnet hdr | pos1 u8 | pos2 u8.
///
/// Two legacy call sites in src/bnetd/command.cpp: /f promote and /f demote.

extern "C" {

int pvpgn_v3_send_friendmove_ack(void* conn_ptr,
                                  unsigned int pos1,
                                  unsigned int pos2) noexcept;

}  // extern "C"
