// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// v3 bridge: encode SERVER_CLAN_MOTDREPLY (0x7c) and send via the packet
// pipeline. Returns 1 on full v3 success (caller skips legacy send), 0
// otherwise. `motd` may be nullptr (treated as empty string).
int pvpgn_v3_send_clan_motdreply(void*        conn_ptr,
                                  unsigned int cookie,
                                  unsigned int unknown1,
                                  char const*  motd) noexcept;

#ifdef __cplusplus
}
#endif
