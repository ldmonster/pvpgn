// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file bnetd_lifecycle_bridges_r247.hpp
/// R247 -- third batch of bnetd small-module observation bridges:
///
///   alias_command : aliasfile_load(filename), aliasfile_unload(),
///                   handle_alias_command(sd, text)
///   command_groups: command_groups_load(filename),
///                   command_groups_unload(),
///                   command_groups_reload(filename)
///   anongame_maplists: anongame_maplists_create(),
///                      anongame_maplists_destroy(),
///                      anongame_tournament_maplists_destroy()
///   handle_udp    : handle_udp_packet(usock, src_addr, src_port)
///
/// Each function logs to its own `v3_bnetd_<module>_bridge`
/// module string. All bridges take POD scalars / null-safe C
/// strings only. Contract: return 0; legacy MUST fall through.

// alias_command
extern "C" int pvpgn_v3_bnetd_aliasfile_load_try(
    const char* filename) noexcept;
extern "C" int pvpgn_v3_bnetd_aliasfile_unload_try(void) noexcept;
extern "C" int pvpgn_v3_bnetd_handle_alias_command_try(
    int sd, const char* text) noexcept;

// command_groups
extern "C" int pvpgn_v3_bnetd_command_groups_load_try(
    const char* filename) noexcept;
extern "C" int pvpgn_v3_bnetd_command_groups_unload_try(void) noexcept;
extern "C" int pvpgn_v3_bnetd_command_groups_reload_try(
    const char* filename) noexcept;

// anongame_maplists
extern "C" int pvpgn_v3_bnetd_anongame_maplists_create_try(void) noexcept;
extern "C" int pvpgn_v3_bnetd_anongame_maplists_destroy_try(void) noexcept;
extern "C" int pvpgn_v3_bnetd_anongame_tournament_maplists_destroy_try(
    void) noexcept;

// handle_udp
extern "C" int pvpgn_v3_bnetd_handle_udp_packet_try(
    int usock, unsigned int src_addr, unsigned int src_port) noexcept;
