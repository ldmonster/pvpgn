// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_listreply_bridges.hpp
/// Strangler-fig hooks for variable-length d2cs->client list replies:
///   * D2CS_CLIENT_GAMELISTREPLY (0x05)  — emitted per public game
///   * D2CS_CLIENT_GAMEINFOREPLY (0x06)  — one packet per joined game

#include <cstdint>

extern "C" {

/// One entry of the public-game listing.
///   hdr | seqno u16 | token u32 | currchar u8 | gameflag u32 |
///   game_name\0 | game_desc\0
/// For the terminator entry the legacy code emits three c-strings; pass
/// terminator=1 to write the extra trailing "\0".
int pvpgn_v3_d2cs_send_gamelistreply(void*        conn_ptr,
                                      unsigned int seqno,
                                      std::uint32_t token,
                                      unsigned int currchar,
                                      std::uint32_t gameflag,
                                      char const*  game_name,
                                      char const*  game_desc,
                                      int          terminator) noexcept;

/// Game info reply:
///   hdr | seqno u16 | gameflag u32 | etime u32 |
///   charlevel u8 | leveldiff u8 | maxchar u8 | currchar u8 |
///   chclass[16] | level[16] |
///   game_desc\0 | currchar * char_names[i]\0
/// `chclass_array` and `level_array` must be 16-byte buffers (legacy
/// fixed-size table). `char_names` is an array of `currchar` cstrings.
int pvpgn_v3_d2cs_send_gameinforeply(void*           conn_ptr,
                                      unsigned int    seqno,
                                      std::uint32_t   gameflag,
                                      std::uint32_t   etime,
                                      unsigned int    charlevel,
                                      unsigned int    leveldiff,
                                      unsigned int    maxchar,
                                      unsigned int    currchar,
                                      unsigned char const* chclass_array,
                                      unsigned char const* level_array,
                                      char const*     game_desc,
                                      char const* const* char_names) noexcept;

}  // extern "C"
