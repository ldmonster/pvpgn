// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_gameinforeply_bnetd_bridge.hpp
/// Strangler-fig hook for D2CS_BNETD_GAMEINFOREPLY (type 0x12)
/// emitted in `on_bnetd_gameinforeq` (handle_bnetd.cpp). Wire:
/// 8-byte hdr | gamename \0 | difficulty u8.

extern "C" {

int pvpgn_v3_d2cs_send_gameinforeply_bnetd(void*        conn_ptr,
                                             unsigned int seqno,
                                             char const*  gamename,
                                             unsigned int difficulty) noexcept;

}  // extern "C"
