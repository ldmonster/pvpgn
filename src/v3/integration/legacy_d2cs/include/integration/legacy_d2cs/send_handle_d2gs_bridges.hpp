// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_handle_d2gs_bridges.hpp
/// Strangler-fig hooks for the 5 d2cs->d2gs internal packet sites in
/// `src/d2cs/handle_d2gs.cpp`. All messages share the 8-byte
/// d2cs/d2gs header (size LE16 | type LE16 | seqno LE32).
///
/// * AUTHREQ      (0x10) -- sessionnum u32 | signlen u32 | realm\0
/// * AUTHREPLY    (0x11) -- reply u32
/// * SETGSINFO    (0x12) -- maxgame u32 | gameflag u32
/// * SETINITINFO  (0x15) -- time u32 | gs_id u32 | ac_version u32 |
///                          ac_checksum\0 | ac_string\0
/// * SETCONFFILE  (0x16) -- size u32 | reserved1 u32 | raw bytes

#include <cstddef>

extern "C" {

int pvpgn_v3_d2cs_send_authreq_d2gs(void*        conn_ptr,
                                      unsigned int seqno,
                                      unsigned int sessionnum,
                                      unsigned int signlen,
                                      char const*  realmname) noexcept;

int pvpgn_v3_d2cs_send_authreply_d2gs(void*        conn_ptr,
                                        unsigned int seqno,
                                        unsigned int reply) noexcept;

int pvpgn_v3_d2cs_send_setgsinfo_d2gs(void*        conn_ptr,
                                        unsigned int seqno,
                                        unsigned int maxgame,
                                        unsigned int gameflag) noexcept;

int pvpgn_v3_d2cs_send_setinitinfo_d2gs(void*        conn_ptr,
                                          unsigned int seqno,
                                          unsigned int time_value,
                                          unsigned int gs_id,
                                          unsigned int ac_version,
                                          char const*  ac_checksum,
                                          char const*  ac_string) noexcept;

int pvpgn_v3_d2cs_send_setconffile_d2gs(void*        conn_ptr,
                                          unsigned int seqno,
                                          unsigned int size_field,
                                          unsigned int reserved1,
                                          void const*  data,
                                          unsigned int data_size) noexcept;

}  // extern "C"
