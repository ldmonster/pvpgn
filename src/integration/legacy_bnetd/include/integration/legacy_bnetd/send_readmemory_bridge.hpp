// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_readmemory_bridge.hpp
/// Strangler-fig hook for SERVER_READMEMORY (SID 0x17): anti-cheat probe.
/// Wire: hdr | request_id u32 LE | address u32 LE | length u32 LE.

extern "C" {

int pvpgn_v3_send_readmemory(void* conn_ptr,
                              unsigned int request_id,
                              unsigned int address,
                              unsigned int length) noexcept;

}  // extern "C"
