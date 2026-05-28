// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file bnetd_lifecycle_bridges_r246.hpp
/// R246 -- second batch of small-module observation bridges:
///
///   i18n        : i18n_load(), i18n_reload()
///   icons       : customicons_load(filename), customicons_unload()
///   attrlayer   : attrlayer_init(), attrlayer_cleanup(),
///                 attrlayer_save(flags), attrlayer_flush(flags)
///   tracker     : tracker_set_servers(servers),
///                 tracker_send_report()
///   team        : teamlist_load(), teamlist_unload()
///   udptest     : udptest_send(sd)
///
/// Each function logs to its own `v3_bnetd_<module>_bridge`
/// module string. All bridges take POD scalars / null-safe C
/// strings only. Contract: return 0; legacy MUST fall through.

// i18n
extern "C" int pvpgn_v3_bnetd_i18n_load_try(void) noexcept;
extern "C" int pvpgn_v3_bnetd_i18n_reload_try(void) noexcept;

// icons
extern "C" int pvpgn_v3_bnetd_icons_load_try(
    const char* filename) noexcept;
extern "C" int pvpgn_v3_bnetd_icons_unload_try(void) noexcept;

// attrlayer
extern "C" int pvpgn_v3_bnetd_attrlayer_init_try(void) noexcept;
extern "C" int pvpgn_v3_bnetd_attrlayer_cleanup_try(void) noexcept;
extern "C" int pvpgn_v3_bnetd_attrlayer_save_try(int flags) noexcept;
extern "C" int pvpgn_v3_bnetd_attrlayer_flush_try(int flags) noexcept;

// tracker
extern "C" int pvpgn_v3_bnetd_tracker_set_servers_try(
    const char* servers) noexcept;
extern "C" int pvpgn_v3_bnetd_tracker_send_report_try(void) noexcept;

// team
extern "C" int pvpgn_v3_bnetd_team_load_try(void) noexcept;
extern "C" int pvpgn_v3_bnetd_team_unload_try(void) noexcept;

// udptest
extern "C" int pvpgn_v3_bnetd_udptest_send_try(int sd) noexcept;
