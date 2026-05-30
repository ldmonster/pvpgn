/*
 * Copyright (C) 2001  Marco Ziech (mmz@gmx.net)
 * Copyright (C) 2005  Bryan Biedenkapp (gatekeep@gmail.com)
 * Copyright (C) 2006,2007,2008  Pelish (pelish@gmail.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "handle_wol/wol_internal.h"

//
// R207: relocated from `src/bnetd/handle_wol.cpp`.
// Strangler-fig move into the v3 `integration_legacy_bnetd_linked`
// library. Symbol surface preserved verbatim. The legacy
// `#ifdef PVPGN_V3_BNETD_INTEGRATION` guards are dropped because
// we are unconditionally inside the v3 build here.
// Note: `irc_*` symbols are still resolved from `bnetd_legacy`
// (irc.cpp) across the bnetd <-> bnetd_legacy link edge; an
// irc.cpp relocation is deferred to a future round.

// Strangler-fig hook for the bnetd WoL (Westwood Online) dispatcher.
// Observation-only: logs each entry to handle_wol_con_command /
// handle_wol_log_command / handle_wol_welcome. Returns 0; legacy
// path always runs.
extern "C" int pvpgn_v3_wol_dispatch_try(void* conn_ptr, char const* op) noexcept;
// Send-bridge: encodes a raw-text packet and dispatches via send_packet handler.
// Returns 1 (handled), 0 (fall through), -1 (error).
extern "C" int pvpgn_v3_send_raw_text(void* conn_ptr, char const* text) noexcept;

namespace pvpgn
{

	namespace bnetd
	{

		typedef int(*t_wol_command)(t_connection * conn, int numparams, char ** params, char * text);

		typedef struct {
			const char     * wol_command_string;
			t_wol_command    wol_command_handler;
		} t_wol_command_table_row;

		/* state "connected" handlers */
		static const t_wol_command_table_row wol_con_command_table[] =
		{
			{ "NICK", _handle_nick_command },
			{ "USER", _handle_user_command },
			{ "PING", _handle_ping_command },
			{ "PONG", _handle_pong_command },
			{ "PASS", _handle_pass_command },
			{ "PRIVMSG", _handle_privmsg_command },
			{ "QUIT", _handle_quit_command },

			{ "CVERS", _handle_cvers_command },
			{ "VERCHK", _handle_verchk_command },
			{ "APGAR", _handle_apgar_command },
			{ "SETOPT", _handle_setopt_command },
			{ "SERIAL", _handle_serial_command },

			/* Ladder server commands */
			{ "LISTSEARCH", _handle_listsearch_command },
			{ "RUNGSEARCH", _handle_rungsearch_command },
			{ "HIGHSCORE", _handle_highscore_command },

			{ NULL, NULL }
		};

		/* state "logged in" handlers */
		static const t_wol_command_table_row wol_log_command_table[] =
		{
			{ "LIST", _handle_list_command },
			{ "TOPIC", _handle_topic_command },
			{ "JOIN", _handle_join_command },
			{ "NAMES", _handle_names_command },
			{ "PART", _handle_part_command },

			{ "SQUADINFO", _handle_squadinfo_command },
			{ "CLANBYNAME", _handle_clanbyname_command },
			{ "SETCODEPAGE", _handle_setcodepage_command },
			{ "SETLOCALE", _handle_setlocale_command },
			{ "GETCODEPAGE", _handle_getcodepage_command },
			{ "GETLOCALE", _handle_getlocale_command },
			{ "GETINSIDER", _handle_getinsider_command },
			{ "JOINGAME", _handle_joingame_command },
			{ "GAMEOPT", _handle_gameopt_command },
			{ "FINDUSER", _handle_finduser_command },
			{ "FINDUSEREX", _handle_finduserex_command },
			{ "PAGE", _handle_page_command },
			{ "STARTG", _handle_startg_command },
			{ "ADVERTR", _handle_advertr_command },
			{ "ADVERTC", _handle_advertc_command },
			{ "CHANCHK", _handle_chanchk_command },
			{ "GETBUDDY", _handle_getbuddy_command },
			{ "ADDBUDDY", _handle_addbuddy_command },
			{ "DELBUDDY", _handle_delbuddy_command },
			{ "TIME", _handle_time_command },
			{ "KICK", _handle_kick_command },
			{ "MODE", _handle_mode_command },
			{ "HOST", _handle_host_command },
			{ "INVMSG", _handle_invmsg_command },
			{ "INVDEL", _handle_invdel_command },
			{ "USERIP", _handle_userip_command },

			{ NULL, NULL }
		};

		extern int handle_wol_con_command(t_connection * conn, char const * command, int numparams, char ** params, char * text)
		{
			(void)pvpgn_v3_wol_dispatch_try(conn, "con_command");
			t_wol_command_table_row const *p;

			for (p = wol_con_command_table; p->wol_command_string != NULL; p++) {
				if (strcasecmp(command, p->wol_command_string) == 0) {
					if (p->wol_command_handler != NULL)
						return ((p->wol_command_handler)(conn, numparams, params, text));
				}
			}
			return -1;
		}

		extern int handle_wol_log_command(t_connection * conn, char const * command, int numparams, char ** params, char * text)
		{
			(void)pvpgn_v3_wol_dispatch_try(conn, "log_command");
			t_wol_command_table_row const *p;

			for (p = wol_log_command_table; p->wol_command_string != NULL; p++) {
				if (strcasecmp(command, p->wol_command_string) == 0) {
					if (p->wol_command_handler != NULL)
						return ((p->wol_command_handler)(conn, numparams, params, text));
				}
			}
			return -1;
		}

		static int handle_wol_authenticate(t_connection * conn, char const * passhash)
		{
			t_account * a;
			char const * tempapgar;
			char const * temphash;
			char const * username;

			if (!conn) {
				ERROR0("got NULL connection");
				return 0;
			}
			if (!passhash) {
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL passhash");
				return 0;
			}
			username = conn_get_loggeduser(conn);
			if (!username) {
				/* redundant sanity check */
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL conn->protocol.loggeduser");
				return 0;
			}
			a = accountlist_find_account(username);
			if (!a) {
				/* FIXME: Send real error code */
				message_send_text(conn, message_type_notice, NULL, "Authentication failed.");
				return 0;
			}
			tempapgar = conn_wol_get_apgar(conn);
			temphash = account_get_wol_apgar(a);

			if (connlist_find_connection_by_account(a) && prefs_v3::kick_old_login() == 0)
			{
				irc_send(conn, ERR_NICKNAMEINUSE, std::string(std::string(conn_get_loggeduser(conn)) + " :Account is already in use!").c_str());
			}
			else if (account_get_auth_lock(a) == 1)
			{
				/* FIXME: Send real error code */
				message_send_text(conn, message_type_notice, NULL, "Authentication rejected (account is locked) ");
			}
			else {
				if (!temphash) {
					/* Account auto creating */
					account_set_wol_apgar(a, tempapgar);
					temphash = account_get_wol_apgar(a);
				}
				if ((tempapgar) && (temphash) && (std::strcmp(temphash, tempapgar) == 0)) {
					/* LOGIN is OK. We sends motd */
					conn_login(conn, a, username);
					conn_set_state(conn, conn_state_loggedin);
					irc_send_motd(conn);
				}
				else {
					irc_send(conn, RPL_BAD_LOGIN, ":You have specified an invalid password for that nickname."); /* bad APGAR */
					conn_increment_passfail_count(conn);
					//std::sprintf(temp,":Closing Link %s[Some.host]:(Password needed for that nickname.)",conn_get_loggeduser(conn));
					//message_send_text(conn,message_type_error,conn,temp);
				}
			}
			return 0;
		}

		extern int handle_wol_welcome(t_connection * conn)
		{
			(void)pvpgn_v3_wol_dispatch_try(conn, "welcome");
			/* This function need rewrite */
			conn_set_state(conn, conn_state_bot_password);

			if (conn_wol_get_apgar(conn)) {
				handle_wol_authenticate(conn, conn_wol_get_apgar(conn));
			}
			else {
				message_send_text(conn, message_type_notice, NULL, "No APGAR command received!");
			}

			return 0;
		}

		static int handle_wol_send_claninfo(t_connection * conn, t_clan * clan)
		{
			unsigned int clanid;
			const char * clantag;
			const char * clanname;

			if (!conn)
			{
				ERROR0("got NULL connection");
				return -1;
			}

			if (clan)
			{
				clanid = clan_get_clanid(clan);
				clantag = clantag_to_str(clan_get_clantag(clan));
				clanname = clan_get_name(clan);
				irc_send(conn, RPL_BATTLECLAN, std::string(std::to_string(clanid) + "`" + std::string(clanname) + "`" + std::string(clantag) + "`0`0`1`0`0`0`0`0`0`0`x`x`x").c_str());
			}
			else
			{
				irc_send(conn, ERR_IDNOEXIST, ":ID does not exist");
			}

			return 0;
		}

		/* Commands: */

	}

}
