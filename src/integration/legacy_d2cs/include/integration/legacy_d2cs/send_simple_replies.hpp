// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_simple_replies.hpp
/// Strangler-fig hooks for simple D2CS->client replies that lack
/// codec encoders. Each builds the 3-byte client header + payload
/// directly via Writer and sends through pvpgn_v3_d2cs_send_packet_try.

#include <cstddef>

extern "C" {

/// D2CS_CLIENT_DELETECHARREPLY (0x0a): u16 u1 + u32 reply.
int pvpgn_v3_d2cs_send_deletecharreply(void*        conn_ptr,
                                        unsigned int reply) noexcept;

/// D2CS_CLIENT_MOTDREPLY (0x12): u8 u1 + message c-string.
int pvpgn_v3_d2cs_send_motdreply(void*       conn_ptr,
                                  char const* message) noexcept;

/// D2CS_CLIENT_CREATEGAMEWAIT (0x14): u32 position.
int pvpgn_v3_d2cs_send_creategamewait(void*        conn_ptr,
                                       unsigned int position) noexcept;

/// D2CS_CLIENT_CONVERTCHARREPLY (0x18): u32 reply.
int pvpgn_v3_d2cs_send_convertcharreply(void*        conn_ptr,
                                         unsigned int reply) noexcept;

}  // extern "C"
