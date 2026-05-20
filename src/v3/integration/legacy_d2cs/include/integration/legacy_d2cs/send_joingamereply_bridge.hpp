// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_joingamereply_bridge.hpp
/// Strangler-fig hook for D2CS_CLIENT_JOINGAMEREPLY (type 0x04).
/// Single call site in handle_d2gs.cpp::on_d2gs_joingamereply.

#include <cstdint>

extern "C" {

/// Build a D2CS_CLIENT_JOINGAMEREPLY (18-byte body: seqno u16,
/// gameid u16, u1 u16, addr u32, token u32, reply u32).
///
/// `addr_host` is the IPv4 address as the legacy code holds it
/// (the value previously passed to `bn_int_nset`, i.e. native
/// host-order interpretation of the bytes obtained from
/// `d2gs_get_ip`/`trans_net`). The bridge writes it on the wire
/// in big-endian (network) order to preserve byte-for-byte parity
/// with `bn_int_nset`.
///
/// Returns:
///   * 1 -- handled.
///   * 0 -- not handled (legacy fallback runs).
int pvpgn_v3_d2cs_send_joingamereply(void*         conn_ptr,
                                      unsigned int  seqno,
                                      unsigned int  gameid,
                                      unsigned int  u1,
                                      std::uint32_t addr_host,
                                      std::uint32_t token,
                                      unsigned int  reply) noexcept;

}  // extern "C"
