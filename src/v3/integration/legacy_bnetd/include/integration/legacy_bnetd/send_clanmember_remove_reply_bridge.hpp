// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_clanmember_remove_reply_bridge.hpp
/// Strangler-fig hook for SERVER_CLANMEMBER_REMOVE_REPLY (SID_CLANMEMBER_REMOVE,
/// 0x78). Single-recipient reply emitted at the end of
/// `_client_clanmember_removereq` after the (optional) 0x7E broadcast notify.
///
/// Wire layout (9 bytes total):
///   header(4)  =  ff 78 09 00
///   count(u32 LE)
///   result(u8)         -- SERVER_CLANMEMBER_REMOVE_SUCCESS/FAILED

#include <cstdint>

extern "C" {

int pvpgn_v3_send_clanmember_remove_reply(void*         conn_ptr,
                                           std::uint32_t count,
                                           std::uint8_t  result) noexcept;

}  // extern "C"
