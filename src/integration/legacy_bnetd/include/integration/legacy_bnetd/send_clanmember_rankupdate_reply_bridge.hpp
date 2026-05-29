// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_clanmember_rankupdate_reply_bridge.hpp
/// Strangler-fig hook for SERVER_CLANMEMBER_RANKUPDATE_REPLY
/// (SID_CLANMEMBER_RANKUPDATE, 0x7A). Single-recipient reply emitted by
/// `_client_clanmember_rankupdatereq` after the rank-update decision.
///
/// Wire layout (9 bytes total):
///   header(4)  =  ff 7a 09 00
///   count(u32 LE)
///   result(u8)         -- SERVER_CLANMEMBER_RANKUPDATE_SUCCESS/FAILED

#include <cstdint>

extern "C" {

int pvpgn_v3_send_clanmember_rankupdate_reply(void*         conn_ptr,
                                               std::uint32_t count,
                                               std::uint8_t  result) noexcept;

}  // extern "C"
