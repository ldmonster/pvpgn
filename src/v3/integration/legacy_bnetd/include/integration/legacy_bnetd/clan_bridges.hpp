// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// v3 bridge: encode SERVER_CLANMEMBERLIST_REPLY (SID 0x7D) into a
// caller-supplied buffer. Variable list of members supplied as parallel
// arrays. Wire layout after the 4-byte BNet header:
//   cookie u32
//   member_count u8
//   per member: name\0 + status u8 + online_flag u8 + online_status\0
//
// `member_count` is the size of the parallel arrays. `online_status[i]`
// may be nullptr (encoded as empty cstring). Names must be non-null and
// non-empty.
//
// Returns 1 on success, 0 on any failure (null arg, encode error,
// buffer too small).
int pvpgn_v3_encode_clanmemberlist_reply(unsigned int          cookie,
                                          unsigned int          member_count,
                                          char const* const*    names,
                                          unsigned char const*  statuses,
                                          unsigned char const*  online_flags,
                                          char const* const*    online_statuses,
                                          unsigned char*        out_buf,
                                          unsigned int          max_size,
                                          unsigned int*         out_size) noexcept;

// v3 bridge: encode SERVER_CLAN_CREATEREPLY (SID 0x70) into a
// caller-supplied buffer. Wire layout after BNet header:
//   cookie u32
//   check_result u8
//   friend_count u8
//   per friend: name\0
int pvpgn_v3_encode_clan_createreply(unsigned int       cookie,
                                      unsigned char      check_result,
                                      unsigned int       friend_count,
                                      char const* const* friend_names,
                                      unsigned char*     out_buf,
                                      unsigned int       max_size,
                                      unsigned int*      out_size) noexcept;

// v3 bridge: send SERVER_CLAN_CLANACK (SID 0x75) to one connection.
// Fixed wire layout: unknown1 u8 (0), clantag u32, status u8.
// Returns 1 on full v3 success (caller skips legacy send), 0 otherwise.
int pvpgn_v3_send_clan_clanack(void*         conn_ptr,
                                unsigned char unknown1,
                                unsigned int  clantag,
                                unsigned char status) noexcept;

// v3 bridge: encode SERVER_CLAN_CLANACK (SID 0x75) wire bytes into a
// caller-supplied buffer. Used by per-member iteration sites that send
// one packet per online clan member with a member-specific status byte.
int pvpgn_v3_encode_clan_clanack(unsigned char  unknown1,
                                  unsigned int   clantag,
                                  unsigned char  status,
                                  unsigned char* out_buf,
                                  unsigned int   max_size,
                                  unsigned int*  out_size) noexcept;

// v3 bridge: encode SERVER_CLANQUITNOTIFY (SID 0x76) wire bytes.
// Fixed layout: status u8.
int pvpgn_v3_encode_clan_quitnotify(unsigned char  status,
                                     unsigned char* out_buf,
                                     unsigned int   max_size,
                                     unsigned int*  out_size) noexcept;

// v3 bridge: send SERVER_CLANQUITNOTIFY (SID 0x76) to one connection.
// Returns 1 on full v3 success (caller skips legacy send), 0 otherwise.
int pvpgn_v3_send_clan_quitnotify(void*         conn_ptr,
                                   unsigned char status) noexcept;

#ifdef __cplusplus
}
#endif
