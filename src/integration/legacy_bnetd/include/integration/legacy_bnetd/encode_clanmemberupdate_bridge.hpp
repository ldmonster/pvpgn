// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// v3 bridge: encode SERVER_CLANMEMBERUPDATE (0x7f) wire bytes into the
// caller-supplied buffer. Unlike the per-connection send bridges this
// helper does NOT dispatch — it only produces the bytes, leaving the
// broadcast iteration to the caller (legacy clan.cpp owns the
// `clan->members` list and cannot expose its types to v3).
//
// Wire layout after the 4-byte BNet header:
//   name           cstring (NUL terminated)
//   status         u8
//   online_flag    u8
//   online_status  cstring (NUL terminated; empty string yields just "\0")
//
// Parameters:
//   name           Player name. Required (non-null, non-empty).
//   status         Clan-member status byte (0..4).
//   online_flag    Online indicator byte (0=offline, non-zero=online).
//   online_status  Optional clienttag string; may be nullptr (encoded as "").
//   out_buf        Destination byte buffer. Required.
//   max_size       Capacity of `out_buf` in bytes. Must be >= produced size.
//   out_size       On success, receives the number of bytes written. Required.
//
// Returns 1 on full encode success, 0 on any failure (null arg, empty
// name, encoder error, oversize result, buffer too small). On failure
// `*out_size` is left untouched. The caller is expected to fall back to
// the legacy broadcast path on a 0 return.
int pvpgn_v3_encode_clanmemberupdate(char const*    name,
                                     unsigned char  status,
                                     unsigned char  online_flag,
                                     char const*    online_status,
                                     unsigned char* out_buf,
                                     unsigned int   max_size,
                                     unsigned int*  out_size) noexcept;

#ifdef __cplusplus
}
#endif
