// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_frienddel_ack_bridge.hpp
/// Strangler-fig hook for SERVER_FRIENDDEL_ACK (SID 0x68).
/// Wire: 4-byte bnet hdr | friend_num u8.
///
/// Single legacy call site: src/bnetd/command.cpp /f r handler.

extern "C" {

int pvpgn_v3_send_frienddel_ack(void* conn_ptr,
                                 unsigned int friend_num) noexcept;

}  // extern "C"
