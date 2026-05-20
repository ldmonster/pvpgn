// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_requiredwork_bridge.hpp
/// Strangler-fig hook for SERVER_REQUIREDWORK (SID 0x4C):
/// anti-cheat extra-work probe. Wire: hdr | filename \0.

extern "C" {

int pvpgn_v3_send_requiredwork(void* conn_ptr,
                                char const* filename) noexcept;

}  // extern "C"
