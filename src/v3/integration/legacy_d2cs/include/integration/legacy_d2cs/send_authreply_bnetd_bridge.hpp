// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_authreply_bnetd_bridge.hpp
/// Strangler-fig hook for D2CS_BNETD_AUTHREPLY (type 0x02) emitted
/// in `on_bnetd_authreq` (handle_bnetd.cpp). Wire: 8-byte hdr
/// (size LE16 | type LE16 | seqno LE32) | version LE32 | realmname \0.

extern "C" {

int pvpgn_v3_d2cs_send_authreply_bnetd(void*        conn_ptr,
                                         unsigned int seqno,
                                         unsigned int version,
                                         char const*  realmname) noexcept;

}  // extern "C"
