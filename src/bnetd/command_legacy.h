// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Forward declarations for legacy chat command handlers that have
// been exposed (de-static'd in command.cpp) so the R216 strangler
// bridge can delegate to them while v3 still owns the dispatch.
//
// As each command is reimplemented in pure v3, the corresponding
// declaration here should be removed and the legacy definition
// deleted from command.cpp.

#ifndef JUST_NEED_TYPES
#  define JUST_NEED_TYPES
#  include "connection.h"
#  undef JUST_NEED_TYPES
#endif

namespace pvpgn
{
    namespace bnetd
    {
        extern int _handle_status_command(t_connection* c, char const* text);
        extern int _handle_who_command(t_connection* c, char const* text);
        extern int _handle_whoami_command(t_connection* c, char const* text);
        extern int _handle_finger_command(t_connection* c, char const* text);
        // R220:
        extern int _handle_time_command(t_connection* c, char const* text);
        extern int _handle_news_command(t_connection* c, char const* text);
        extern int _handle_games_command(t_connection* c, char const* text);
        extern int _handle_channels_command(t_connection* c, char const* text);
        extern int _handle_motd_command(t_connection* c, char const* text);
        // R221:
        extern int _handle_copyright_command(t_connection* c, char const* text);
        extern int _handle_lusers_command(t_connection* c, char const* text);
        extern int _handle_connections_command(t_connection* c, char const* text);
        extern int _handle_admins_command(t_connection* c, char const* text);
        // R222: per-session state commands.
        extern int _handle_quit_command(t_connection* c, char const* text);
        extern int _handle_beep_command(t_connection* c, char const* text);
        extern int _handle_nobeep_command(t_connection* c, char const* text);
        extern int _handle_away_command(t_connection* c, char const* text);
        extern int _handle_dnd_command(t_connection* c, char const* text);
        extern int _handle_squelch_command(t_connection* c, char const* text);
        extern int _handle_unsquelch_command(t_connection* c, char const* text);
        // R223: social/messaging commands.
        extern int _handle_clan_command(t_connection* c, char const* text);
        extern int _handle_friends_command(t_connection* c, char const* text);
        extern int _handle_me_command(t_connection* c, char const* text);
        extern int _handle_whisper_command(t_connection* c, char const* text);
        extern int _handle_watch_command(t_connection* c, char const* text);
        extern int _handle_unwatch_command(t_connection* c, char const* text);
        extern int _handle_tos_command(t_connection* c, char const* text);
        extern int _handle_clearstats_command(t_connection* c, char const* text);
        // R224: channel/chat-ops commands.
        extern int _handle_channel_command(t_connection* c, char const* text);
        extern int _handle_rejoin_command(t_connection* c, char const* text);
        extern int _handle_topic_command(t_connection* c, char const* text);
        extern int _handle_moderate_command(t_connection* c, char const* text);
        extern int _handle_announce_command(t_connection* c, char const* text);
        extern int _handle_reply_command(t_connection* c, char const* text);
        extern int _handle_realmann_command(t_connection* c, char const* text);
        extern int _handle_watchall_command(t_connection* c, char const* text);
        extern int _handle_unwatchall_command(t_connection* c, char const* text);
        extern int _handle_alert_command(t_connection* c, char const* text);
        // R225: channel rights / op commands.
        extern int _handle_admin_command(t_connection* c, char const* text);
        extern int _handle_operator_command(t_connection* c, char const* text);
        extern int _handle_aop_command(t_connection* c, char const* text);
        extern int _handle_op_command(t_connection* c, char const* text);
        extern int _handle_tmpop_command(t_connection* c, char const* text);
        extern int _handle_deop_command(t_connection* c, char const* text);
        extern int _handle_voice_command(t_connection* c, char const* text);
        extern int _handle_devoice_command(t_connection* c, char const* text);
        extern int _handle_vop_command(t_connection* c, char const* text);
        // R226: moderation / account-state commands.
        extern int _handle_kick_command(t_connection* c, char const* text);
        extern int _handle_ban_command(t_connection* c, char const* text);
        extern int _handle_unban_command(t_connection* c, char const* text);
        extern int _handle_lockacct_command(t_connection* c, char const* text);
        extern int _handle_unlockacct_command(t_connection* c, char const* text);
        extern int _handle_muteacct_command(t_connection* c, char const* text);
        extern int _handle_unmuteacct_command(t_connection* c, char const* text);
        extern int _handle_flag_command(t_connection* c, char const* text);
        extern int _handle_tag_command(t_connection* c, char const* text);
        // R227: account / admin-ops commands.
        extern int _handle_addacct_command(t_connection* c, char const* text);
        extern int _handle_chpass_command(t_connection* c, char const* text);
        extern int _handle_kill_command(t_connection* c, char const* text);
        extern int _handle_killsession_command(t_connection* c, char const* text);
        extern int _handle_find_command(t_connection* c, char const* text);
        extern int _handle_save_command(t_connection* c, char const* text);
        extern int _handle_set_command(t_connection* c, char const* text);
        extern int _handle_rehash_command(t_connection* c, char const* text);
        extern int _handle_config_command(t_connection* c, char const* text);
        extern int _handle_shutdown_command(t_connection* c, char const* text);
        extern int _handle_serverban_command(t_connection* c, char const* text);
        // R228: info / network / misc commands.
        extern int _handle_stats_command(t_connection* c, char const* text);
        extern int _handle_whois_command(t_connection* c, char const* text);
        extern int _handle_gameinfo_command(t_connection* c, char const* text);
        extern int _handle_ladderactivate_command(t_connection* c, char const* text);
        extern int _handle_ladderinfo_command(t_connection* c, char const* text);
        extern int _handle_timer_command(t_connection* c, char const* text);
        extern int _handle_netinfo_command(t_connection* c, char const* text);
        extern int _handle_quota_command(t_connection* c, char const* text);
        extern int _handle_ipscan_command(t_connection* c, char const* text);
        extern int _handle_commandgroups_command(t_connection* c, char const* text);
        extern int _handle_ping_command(t_connection* c, char const* text);
        // R229: extern bodies in other translation units (mail.cpp,
        // icons.cpp, ipban.cpp, i18n.cpp, userlog.cpp).
        extern int handle_mail_command(t_connection* c, char const* text);
        extern int handle_icon_command(t_connection* c, char const* text);
        extern int handle_ipban_command(t_connection* c, char const* text);
        extern int handle_language_command(t_connection* c, char const* text);
        extern int handle_log_command(t_connection* c, char const* text);
    }
}
