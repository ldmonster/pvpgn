// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_friendadd_ack_bridge.hpp
/// Strangler-fig hook for SERVER_FRIENDADD_ACK (SID 0x67).
/// Wire: 4-byte bnet hdr | name \0 | status u8 | location u8 |
///       client_tag u32 LE | location_name \0.
///
/// Single legacy call site: src/bnetd/command.cpp /f a handler.

extern "C" {

int pvpgn_v3_send_friendadd_ack(void* conn_ptr,
                                 char const* name,
                                 unsigned int status,
                                 unsigned int location,
                                 unsigned int client_tag,
                                 char const* location_name) noexcept;

}  // extern "C"
