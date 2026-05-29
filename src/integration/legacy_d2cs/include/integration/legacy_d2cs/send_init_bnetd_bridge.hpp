// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_init_bnetd_bridge.hpp
/// Strangler-fig hook for the d2cs->bnetd initial handshake packet
/// (1-byte init class = CLIENT_INITCONN_CLASS_D2CS_BNETD = 0x65).

extern "C" {

int pvpgn_v3_d2cs_send_init_bnetd(void* conn_ptr) noexcept;

}  // extern "C"
