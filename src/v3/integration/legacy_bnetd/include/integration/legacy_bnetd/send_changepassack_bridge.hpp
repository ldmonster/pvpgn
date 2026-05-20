// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_changepassack_bridge.hpp
/// Strangler-fig hook for SERVER_CHANGEPASSACK (SID_CHANGEPASSWORD, 0x31).
///
/// `pvpgn_v3_send_changepassack` encodes a `ChangePasswordReply` via the v3
/// codec and dispatches the bytes through the registered send_packet handler.
///
/// Wire layout (8 bytes total):
///   header(4)  =  ff 31 08 00
///   message(u32 LE)        -- SERVER_CHANGEPASSACK_MESSAGE_SUCCESS (1)
///                             or _FAIL (0)
///
/// Covers the single push site at the end of `_client_changepassreq`
/// (handle_bnet.cpp, line ~1527). All decision branches in that handler
/// converge to a single `conn_push_outqueue(c, rpacket)`; the bridge is
/// invoked after the message field is set on rpacket.

#include <cstdint>

extern "C" {

int pvpgn_v3_send_changepassack(void*         conn_ptr,
                                 std::uint32_t message) noexcept;

}  // extern "C"
