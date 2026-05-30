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

#include "common/setup_before.h"
#include "irc.h"

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <strings.h>

#include "common/irc_protocol.h"
#include "common/packet.h"
#include "common/eventlog.h"
#include "common/field_sizes.h"
#include "common/bnethash.h"
#include "common/addr.h"
#include "common/tag.h"
#include "common/list.h"
#include "common/util.h"
#include "common/xstring.h"

#include "message.h"
#include "channel.h"
#include "game.h"
#include "connection.h"
#include "server.h"
#include "account.h"
#include "account_wrap.h"
#include "prefs_v3_shim.h"
#include "tick.h"
#include "handle_wol.h"
// R194: handle_wserv.h + handle_wserv.cpp were deleted in commit
// 03f35f9. The only call site (`handle_wserv_con_command`) is the
// `conn_class_wserv` switch case below. Since the implementation is
// gone the call site has been replaced with a fall-through to the
// generic IRC dispatcher (preserving wserv connections rather than
// link-failing on the missing symbol). See plans/r194-checklist.md.
#include "command_groups.h"
#include "topic.h"
#include "clan.h"
#include "command.h"
#include "anongame_wol.h"
#include "common/setup_after.h"

#include "irc/irc_internal.h"

extern "C" int pvpgn_v3_send_raw_text(void* conn_ptr, char const* text) noexcept;

namespace pvpgn
{

	namespace bnetd
	{

			// ---------------------------------------------------------------------------
			// IRC dispatch tables + handle_irc_common_packet
			// moved from handle_irc_common.cpp (Round 134)
			// ---------------------------------------------------------------------------
	
			typedef int(*t_irc_command)(t_connection * conn, int numparams, char ** params, char * text);
	
			typedef struct {
				const char     * irc_command_string;
				t_irc_command    irc_command_handler;
			} t_irc_command_table_row;
	
			/* state "connected" handlers */
			static const t_irc_command_table_row irc_con_command_table[] =
			{
				{ "NICK",    _handle_nick_command    },
				{ "USER",    _handle_user_command    },
				{ "PING",    _handle_ping_command    },
				{ "PONG",    _handle_pong_command    },
				{ "PASS",    _handle_pass_command    },
				{ "PRIVMSG", _handle_privmsg_command },
				{ "NOTICE",  _handle_notice_command  },
				{ "QUIT",    _handle_quit_command    },
				{ NULL, NULL }
			};
	
			/* state "logged in" handlers */
			static const t_irc_command_table_row irc_log_command_table[] =
			{
				{ "WHO",      _handle_who_command      },
				{ "LIST",     _handle_list_command     },
				{ "TOPIC",    _handle_topic_command    },
				{ "JOIN",     _handle_join_command     },
				{ "NAMES",    _handle_names_command    },
				{ "MODE",     _handle_mode_command     },
				{ "USERHOST", _handle_userhost_command },
				{ "ISON",     _handle_ison_command     },
				{ "WHOIS",    _handle_whois_command    },
				{ "PART",     _handle_part_command     },
				{ "KICK",     _handle_kick_command     },
				{ "TIME",     _handle_time_command     },
				{ NULL, NULL }
			};
	
			static int irc_dispatch_con_command(t_connection * conn, char const * command, int numparams, char ** params, char * text)
			{
				t_irc_command_table_row const *p;
				for (p = irc_con_command_table; p->irc_command_string != NULL; p++) {
					if (strcasecmp(command, p->irc_command_string) == 0) {
						if (p->irc_command_handler != NULL)
							return ((p->irc_command_handler)(conn, numparams, params, text));
					}
				}
				return -1;
			}
	
			static int irc_dispatch_log_command(t_connection * conn, char const * command, int numparams, char ** params, char * text)
			{
				t_irc_command_table_row const *p;
				for (p = irc_log_command_table; p->irc_command_string != NULL; p++) {
					if (strcasecmp(command, p->irc_command_string) == 0) {
						if (p->irc_command_handler != NULL)
							return ((p->irc_command_handler)(conn, numparams, params, text));
					}
				}
				return -1;
			}
	
			static int irc_common_con_command(t_connection * conn, char const * command, int numparams, char ** params, char * text)
			{
				if (!conn) {
					eventlog(eventlog_level_error, __FUNCTION__, "got NULL connection");
					return -1;
				}
				switch (conn_get_class(conn)) {
				case conn_class_irc:
					return irc_dispatch_con_command(conn, command, numparams, params, text);
				case conn_class_wserv:
					// R194: handle_wserv_con_command was deleted in commit 03f35f9.
					// Fall through to the generic IRC dispatcher so wserv connections
					// keep working (no per-class processing remains).
					return irc_dispatch_con_command(conn, command, numparams, params, text);
				case conn_class_wol:
				case conn_class_wladder:
				case conn_class_wgameres:
					return handle_wol_con_command(conn, command, numparams, params, text);
				default:
					return irc_dispatch_con_command(conn, command, numparams, params, text);
				}
			}
	
			static int irc_common_log_command(t_connection * conn, char const * command, int numparams, char ** params, char * text)
			{
				if (!conn) {
					eventlog(eventlog_level_error, __FUNCTION__, "got NULL connection");
					return -1;
				}
				switch (conn_get_class(conn)) {
				case conn_class_irc:
					return irc_dispatch_log_command(conn, command, numparams, params, text);
				case conn_class_wol:
				case conn_class_wgameres:
					return handle_wol_log_command(conn, command, numparams, params, text);
				default:
					return irc_dispatch_log_command(conn, command, numparams, params, text);
				}
			}
	
			static int irc_common_set_class(t_connection * conn, char const * command, int numparams, char ** params, char * text)
			{
				if (!conn) {
					eventlog(eventlog_level_error, __FUNCTION__, "got NULL connection");
					return -1;
				}
				if (conn_get_class(conn) != conn_class_ircinit) {
					DEBUG0("FIXME: conn_get_class(conn) != conn_class_ircinit");
					return -1;
				}
				else {
					if (strcasecmp(command, "VERCHK") == 0) {
						DEBUG0("Got WSERV packet");
						if (std::strcmp(prefs_v3::wolv2_addrs(), "") != 0)
							conn_set_class(conn, conn_class_wserv);
						else
							conn_set_state(conn, conn_state_destroy);
						return 0;
					}
					else if (strcasecmp(command, "CVERS") == 0) {
						DEBUG0("Got WOL packet");
						if ((std::strcmp(prefs_v3::wolv1_addrs(), "") != 0) || (std::strcmp(prefs_v3::wolv2_addrs(), "") != 0))
							conn_set_class(conn, conn_class_wol);
						else
							conn_set_state(conn, conn_state_destroy);
						return 0;
					}
					else if ((strcasecmp(command, "LISTSEARCH") == 0) ||
						(strcasecmp(command, "RUNGSEARCH") == 0) ||
						(strcasecmp(command, "HIGHSCORE") == 0)) {
						DEBUG0("Got WOL Ladder packet");
						if (std::strcmp(prefs_v3::wolv2_addrs(), "") != 0)
							conn_set_class(conn, conn_class_wladder);
						else
							conn_set_state(conn, conn_state_destroy);
						return 0;
					}
					else if ((strcasecmp(command, "CRYPT") == 0) ||
						(strcasecmp(command, "LOGIN") == 0)) {
						DEBUG0("Got GameSpy packet");
						if (std::strcmp(prefs_v3::irc_addrs(), "") != 0)
							conn_set_class(conn, conn_class_irc);
						else
							conn_set_state(conn, conn_state_destroy);
						return 0;
					}
					else {
						DEBUG0("Got IRC packet");
						if (std::strcmp(prefs_v3::irc_addrs(), "") != 0)
							conn_set_class(conn, conn_class_irc);
						else
							conn_set_state(conn, conn_state_destroy);
						return 0;
					}
				}
			}
	
			/* xstrdup-equivalent using new char[] for paired delete[] cleanup. */
			static char* irc_common_strdup(char const* s)
			{
				if (!s) return nullptr;
				std::size_t n = std::strlen(s) + 1;
				char* r = new char[n];
				std::memcpy(r, s, n);
				return r;
			}
	
			static int irc_common_line(t_connection * conn, char const * ircline)
			{
				/* [:prefix] <command> [[param1] [param2] ... [paramN]] [:<text>] */
				char * line;
				char * prefix = NULL;
				char * command;
				char ** params = NULL;
				char * text = NULL;
				char * bnet_command = NULL;
				int unrecognized_before = 0;
				int linelen;
				int numparams = 0;
				char * tempparams;
				int i;
	
				if (!conn) {
					eventlog(eventlog_level_error, __FUNCTION__, "got NULL connection");
					return -1;
				}
				if (!ircline) {
					eventlog(eventlog_level_error, __FUNCTION__, "got NULL ircline");
					return -1;
				}
				if (ircline[0] == '\0') {
					return -1;
				}
	
				if (std::strlen(ircline) > MAX_IRC_MESSAGE_LEN) {
					char * tmp = (char *)ircline;
					eventlog(eventlog_level_warn, __FUNCTION__, "line too long, truncation...");
					tmp[MAX_IRC_MESSAGE_LEN] = '\0';
				}
	
				line = irc_common_strdup(ircline);
	
				if (line[0] == ':') {
					prefix = line;
					if (!(command = std::strchr(line, ' '))) {
						eventlog(eventlog_level_warn, __FUNCTION__, "got malformed line (missing command)");
						delete[] line;
						return -1;
					}
					*command++ = '\0';
				}
				else {
					command = line;
				}
	
				tempparams = std::strchr(command, ' ');
				if (tempparams) {
					*tempparams++ = '\0';
					if (tempparams[0] == ':') {
						text = tempparams + 1;
					}
					else {
						for (i = 0; tempparams[i] != '\0'; i++) {
							if ((tempparams[i] == ' ') && (tempparams[i + 1] == ':')) {
								text = tempparams + i;
								*text++ = '\0';
								text++;
								break;
							}
						}
						params = irc_get_paramelems(tempparams);
					}
				}
	
				if (params) {
					for (numparams = 0; params[numparams]; numparams++);
				}
	
				{
					std::string paramtemp;
					bool first = true;
					for (i = 0; ((numparams > 0) && (params[i])); i++)
					{
						if (first)
							first = false;
						else
							paramtemp.append(" ");
						paramtemp.append("\"" + std::string(params[i]) + "\"");
					}
					eventlog(eventlog_level_debug, __FUNCTION__, "[{}] got \"{}\" \"{}\" [{}] \"{}\"", conn_get_socket(conn), ((prefix) ? (prefix) : ("")), command, paramtemp, ((text) ? (text) : ("")));
				}
	
				if (conn_get_class(conn) == conn_class_ircinit) {
					irc_common_set_class(conn, command, numparams, params, text);
				}
	
				if (conn_get_state(conn) == conn_state_connected) {
					conn_set_state(conn, conn_state_bot_username);
	
					if ((conn_get_class(conn) != conn_class_wserv) &&
						(conn_get_class(conn) != conn_class_wladder)) {
	
						t_timer_data temp;
						temp.n = prefs_v3::irc_latency();
						conn_test_latency(conn, std::time(NULL), temp);
					}
				}
	
				if (irc_common_con_command(conn, command, numparams, params, text) != -1) {}
				else if (conn_get_state(conn) != conn_state_loggedin)
				{
					std::string tmp(":Unrecognized command \"" + std::string(command) + "\" (before login)");
					if (tmp.length() > MAX_IRC_MESSAGE_LEN)
						irc_send(conn, ERR_UNKNOWNCOMMAND, tmp.c_str());
					else
						irc_send(conn, ERR_UNKNOWNCOMMAND, ":Unrecognized command (before login)");
				}
				else
				{
					unrecognized_before = 1;
				}
	
				if ((conn_get_state(conn) == conn_state_loggedin) && (unrecognized_before)) {
					if (irc_common_log_command(conn, command, numparams, params, text) != -1) {}
					else if ((strstart(command, "LAG") != 0) && (strstart(command, "JOIN") != 0)){
						linelen = std::strlen(ircline);
						bnet_command = new char[linelen + 2];
						bnet_command[0] = '/';
						std::strcpy(bnet_command + 1, ircline);
						handle_command(conn, bnet_command);
						delete[] bnet_command;
					}
				}
	
				if (params)
					irc_unget_paramelems(params);
				delete[] line;
				return 0;
			}
	
			extern int handle_irc_common_packet(t_connection * conn, t_packet const * const packet)
			{
				unsigned int i;
				char ircline[MAX_IRC_MESSAGE_LEN];
				char const * data;
	
				if (!packet) {
					eventlog(eventlog_level_error, __FUNCTION__, "got NULL packet");
					return -1;
				}
				if ((conn_get_class(conn) != conn_class_ircinit) &&
					(conn_get_class(conn) != conn_class_irc) &&
					(conn_get_class(conn) != conn_class_wol) &&
					(conn_get_class(conn) != conn_class_wserv) &&
					(conn_get_class(conn) != conn_class_wladder)) {
					eventlog(eventlog_level_error, __FUNCTION__, "FIXME: handle_irc_packet without any reason (conn->class != conn_class_irc/ircinit/wol/wserv...)");
					return -1;
				}
	
				std::memset(ircline, 0, sizeof(ircline));
				data = conn_get_ircline(conn);
				if (data)
					std::snprintf(ircline, sizeof ircline, "%s", data);
				unsigned ircpos = std::strlen(ircline);
				data = (const char *)packet_get_raw_data_const(packet, 0);
	
				for (i = 0; i < packet_get_size(packet); i++) {
					if (data[i] == '\n') {
						irc_common_line(conn, ircline);
						std::memset(ircline, 0, sizeof(ircline));
						ircpos = 0;
					}
					else {
						if (ircpos < MAX_IRC_MESSAGE_LEN - 1)
							ircline[ircpos++] = data[i];
						else {
							ircpos++;
							eventlog(eventlog_level_warn, __FUNCTION__, "[{}] client exceeded maximum allowed message length by {} characters", conn_get_socket(conn), ircpos - MAX_IRC_MESSAGE_LEN);
							if (ircpos > 100 + MAX_IRC_MESSAGE_LEN) {
								eventlog(eventlog_level_error, __FUNCTION__, "[{}] excess flood", conn_get_socket(conn));
								return -1;
							}
						}
					}
				}
				conn_set_ircline(conn, ircline);
				return 0;
			}
	
		}
	
	}
