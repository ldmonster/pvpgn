// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file bnetd_lifecycle_bridges.hpp
/// Unified lifecycle observation bridges for bnetd small-module subsystems.
/// Merged from three revision batches (R245, R246, R247) — all are
/// observation-only (return 0; legacy MUST fall through).
///
/// R245 subsystems:
///   helpfile    : helpfile_init(filename), helpfile_unload()
///   autoupdate  : autoupdate_load(filename), autoupdate_unload()
///   output      : output_init(), output_write_to_file()
///   support     : support_check_files(supportfile)
///   mail        : handle_mail_command(sd, text), check_mail(sd)
///
/// R246 subsystems:
///   i18n        : i18n_load(), i18n_reload()
///   icons       : customicons_load(filename), customicons_unload()
///   attrlayer   : attrlayer_init(), attrlayer_cleanup(),
///                 attrlayer_save(flags), attrlayer_flush(flags)
///   tracker     : tracker_set_servers(servers),
///                 tracker_send_report()
///   team        : teamlist_load(), teamlist_unload()
///   udptest     : udptest_send(sd)
///
/// R247 subsystems:
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
/// Each function logs to its own `v3_bnetd_<module>_bridge` module
/// string so downstream filters can target an individual subsystem.
/// All bridges take POD scalars / null-safe C strings only.
/// Contract: every bridge returns 0 and legacy MUST fall through.

// ---------------------------------------------------------------------------
// R245: helpfile
// ---------------------------------------------------------------------------
extern "C" int pvpgn_v3_bnetd_helpfile_init(
    const char* filename) noexcept;
extern "C" int pvpgn_v3_bnetd_helpfile_unload(void) noexcept;

// ---------------------------------------------------------------------------
// R245: autoupdate
// ---------------------------------------------------------------------------
extern "C" int pvpgn_v3_bnetd_autoupdate_load(
    const char* filename) noexcept;
extern "C" int pvpgn_v3_bnetd_autoupdate_unload(void) noexcept;

// ---------------------------------------------------------------------------
// R245: output
// ---------------------------------------------------------------------------
extern "C" int pvpgn_v3_bnetd_output_init(void) noexcept;
extern "C" int pvpgn_v3_bnetd_output_write_to_file(void) noexcept;

// ---------------------------------------------------------------------------
// R245: support
// ---------------------------------------------------------------------------
extern "C" int pvpgn_v3_bnetd_support_check_files(
    const char* supportfile) noexcept;

// ---------------------------------------------------------------------------
// R245: mail
// ---------------------------------------------------------------------------
/// `sd` -- raw socket descriptor of the issuing connection (or -1).
extern "C" int pvpgn_v3_bnetd_mail_handle_command(
    int sd,
    const char* text) noexcept;
extern "C" int pvpgn_v3_bnetd_mail_check(int sd) noexcept;

// ---------------------------------------------------------------------------
// R246: i18n
// ---------------------------------------------------------------------------
extern "C" int pvpgn_v3_bnetd_i18n_load(void) noexcept;
extern "C" int pvpgn_v3_bnetd_i18n_reload(void) noexcept;

// ---------------------------------------------------------------------------
// R246: icons
// ---------------------------------------------------------------------------
extern "C" int pvpgn_v3_bnetd_icons_load(
    const char* filename) noexcept;
extern "C" int pvpgn_v3_bnetd_icons_unload(void) noexcept;

// ---------------------------------------------------------------------------
// R246: attrlayer
// ---------------------------------------------------------------------------
extern "C" int pvpgn_v3_bnetd_attrlayer_init(void) noexcept;
extern "C" int pvpgn_v3_bnetd_attrlayer_cleanup(void) noexcept;
extern "C" int pvpgn_v3_bnetd_attrlayer_save(int flags) noexcept;
extern "C" int pvpgn_v3_bnetd_attrlayer_flush(int flags) noexcept;

// ---------------------------------------------------------------------------
// R246: tracker
// ---------------------------------------------------------------------------
extern "C" int pvpgn_v3_bnetd_tracker_set_servers(
    const char* servers) noexcept;
extern "C" int pvpgn_v3_bnetd_tracker_send_report(void) noexcept;

// ---------------------------------------------------------------------------
// R246: team
// ---------------------------------------------------------------------------
extern "C" int pvpgn_v3_bnetd_team_load(void) noexcept;
extern "C" int pvpgn_v3_bnetd_team_unload(void) noexcept;

// ---------------------------------------------------------------------------
// R246: udptest
// ---------------------------------------------------------------------------
extern "C" int pvpgn_v3_bnetd_udptest_send(int sd) noexcept;

// ---------------------------------------------------------------------------
// R247: alias_command
// ---------------------------------------------------------------------------
extern "C" int pvpgn_v3_bnetd_aliasfile_load(
    const char* filename) noexcept;
extern "C" int pvpgn_v3_bnetd_aliasfile_unload(void) noexcept;
extern "C" int pvpgn_v3_bnetd_handle_alias_command(
    int sd, const char* text) noexcept;

// ---------------------------------------------------------------------------
// R247: command_groups
// ---------------------------------------------------------------------------
extern "C" int pvpgn_v3_bnetd_command_groups_load(
    const char* filename) noexcept;
extern "C" int pvpgn_v3_bnetd_command_groups_unload(void) noexcept;
extern "C" int pvpgn_v3_bnetd_command_groups_reload(
    const char* filename) noexcept;

// ---------------------------------------------------------------------------
// R247: anongame_maplists
// ---------------------------------------------------------------------------
extern "C" int pvpgn_v3_bnetd_anongame_maplists_create(void) noexcept;
extern "C" int pvpgn_v3_bnetd_anongame_maplists_destroy(void) noexcept;
extern "C" int pvpgn_v3_bnetd_anongame_tournament_maplists_destroy(
    void) noexcept;

// ---------------------------------------------------------------------------
// R247: handle_udp
// ---------------------------------------------------------------------------
extern "C" int pvpgn_v3_bnetd_handle_udp_packet(
    int usock, unsigned int src_addr, unsigned int src_port) noexcept;
