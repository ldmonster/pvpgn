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

#ifdef PVPGN_V3_BNETD_INTEGRATION
// Send-bridge: encodes a raw-text packet and dispatches via send_packet handler.
// Returns 1 (handled), 0 (fall through), -1 (error).
extern "C" int pvpgn_v3_send_raw_text(void* conn_ptr, char const* text) noexcept;
#endif

namespace pvpgn
{

	namespace bnetd
	{


		/* xstrdup-equivalent using new char[] for paired delete[] cleanup. */
		static char* irc_strdup(char const* s)
		{
			if (!s) return nullptr;
			std::size_t n = std::strlen(s) + 1;
			char* r = new char[n];
			std::memcpy(r, s, n);
			return r;
		}
		typedef struct {
			char const * nick;
			char const * user;
			char const * host;
		} t_irc_message_from;


		static char ** irc_split_elems(char * list, int separator, int ignoreblank);
		static int irc_unget_elems(char ** elems);
		static char * irc_message_preformat(t_irc_message_from const * from, char const * command, char const * dest, char const * text);

		extern int irc_send_cmd(t_connection * conn, char const * command, char const * params)
		{
			t_packet * p;
			char data[MAX_IRC_MESSAGE_LEN + 1];
			char const * ircname = server_get_hostname();
			char const * nick;

			if (!conn) {
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL connection");
				return -1;
			}
			if (!command) {
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL command");
				return -1;
			}
			nick = conn_get_loggeduser(conn);
			if (!nick)
				nick = "UserName";

			if (params)
					std::snprintf(data, sizeof(data), ":%s %s %s %s", ircname, command, nick, params);
			else
					std::snprintf(data, sizeof(data), ":%s %s %s", ircname, command, nick);

			DEBUG2("[{}] sent \"{}\"", conn_get_socket(conn), data);
			std::strcat(data, "\r\n");

#ifdef PVPGN_V3_BNETD_INTEGRATION
			{
				int const _rc = pvpgn_v3_send_raw_text(conn, data);
				if (_rc == 1) goto irc_send_cmd_skip_legacy;
				if (_rc == -1) return -1;
			}
#endif
			if (!(p = packet_create(packet_class_raw))) {
				eventlog(eventlog_level_error, __FUNCTION__, "could not create packet");
				return -1;
			}
			packet_set_size(p, 0);
			packet_append_data(p, data, std::strlen(data));
			conn_push_outqueue(conn, p);
			packet_del_ref(p);
#ifdef PVPGN_V3_BNETD_INTEGRATION
			irc_send_cmd_skip_legacy:;
#endif
			return 0;
		}

		extern int irc_send(t_connection * conn, int code, char const * params)
		{
			char temp[4]; /* '000\0' */

			if (!conn) {
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL connection");
				return -1;
			}
			if ((code > 999) || (code < 0)) { /* more than 3 digits or negative */
				eventlog(eventlog_level_error, __FUNCTION__, "invalid message code ({})", code);
				return -1;
			}
			std::sprintf(temp, "%03u", code);
			return irc_send_cmd(conn, temp, params);
		}

		extern int irc_send_ping(t_connection * conn)
		{
			t_packet * p;
			char data[MAX_IRC_MESSAGE_LEN];

			if (!conn) {
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL connection");
				return -1;
			}

			if ((conn_get_class(conn) == conn_class_wserv) ||
				(conn_get_class(conn) == conn_class_wgameres) ||
				(conn_get_class(conn) == conn_class_wladder))
				return 0;

			conn_set_ircping(conn, get_ticks());
			if (conn_get_state(conn) == conn_state_bot_username)
				std::sprintf(data, "PING :%u", conn_get_ircping(conn)); /* Undernet doesn't reveal the servername yet ... neither do we */
			else if ((6 + std::strlen(server_get_hostname()) + 2 + 1) <= MAX_IRC_MESSAGE_LEN)
				std::sprintf(data, "PING :%s", server_get_hostname());
			else
				eventlog(eventlog_level_error, __FUNCTION__, "maximum message length exceeded");
			eventlog(eventlog_level_debug, __FUNCTION__, "[{}] sent \"{}\"", conn_get_socket(conn), data);
			std::strcat(data, "\r\n");

#ifdef PVPGN_V3_BNETD_INTEGRATION
			{
				int const _rc = pvpgn_v3_send_raw_text(conn, data);
				if (_rc == 1) goto irc_send_ping_skip_legacy;
				if (_rc == -1) return -1;
			}
#endif
			if (!(p = packet_create(packet_class_raw))) {
				eventlog(eventlog_level_error, __FUNCTION__, "could not create packet");
				return -1;
			}
			packet_set_size(p, 0);
			packet_append_data(p, data, std::strlen(data));
			conn_push_outqueue(conn, p);
			packet_del_ref(p);
#ifdef PVPGN_V3_BNETD_INTEGRATION
			irc_send_ping_skip_legacy:;
#endif
			return 0;
		}

		extern int irc_send_pong(t_connection * conn, char const * params)
		{
			t_packet * p;
			char data[MAX_IRC_MESSAGE_LEN];

			if (!conn) {
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL connection");
				return -1;
			}
			if ((1 + std::strlen(server_get_hostname()) + 1 + 4 + 1 + std::strlen(server_get_hostname()) + ((params) ? (2 + std::strlen(params)) : (0)) + 2 + 1) > MAX_IRC_MESSAGE_LEN) {
				eventlog(eventlog_level_error, __FUNCTION__, "max message length exceeded");
				return -1;
			}
			if (params)
				std::sprintf(data, ":%s PONG %s :%s", server_get_hostname(), server_get_hostname(), params);
			else
				std::sprintf(data, ":%s PONG %s", server_get_hostname(), server_get_hostname());
			eventlog(eventlog_level_debug, __FUNCTION__, "[{}] sent \"{}\"", conn_get_socket(conn), data);
			std::strcat(data, "\r\n");

#ifdef PVPGN_V3_BNETD_INTEGRATION
			{
				int const _rc = pvpgn_v3_send_raw_text(conn, data);
				if (_rc == 1) goto irc_send_pong_skip_legacy;
				if (_rc == -1) return -1;
			}
#endif
			if (!(p = packet_create(packet_class_raw))) {
				eventlog(eventlog_level_error, __FUNCTION__, "could not create packet");
				return -1;
			}
			packet_set_size(p, 0);
			packet_append_data(p, data, std::strlen(data));
			conn_push_outqueue(conn, p);
			packet_del_ref(p);
#ifdef PVPGN_V3_BNETD_INTEGRATION
			irc_send_pong_skip_legacy:;
#endif
			return 0;
		}

		extern int irc_authenticate(t_connection * conn, char const * passhash)
		{
			/* FIXME: Move this function to handle_irc.* file! */
			char temp[MAX_IRC_MESSAGE_LEN];
			t_hash h1;
			t_hash h2;
			t_account * a;
			char const * temphash;
			char const * username;

			if (!conn) {
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL connection");
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
				message_send_text(conn, message_type_notice, NULL, "Authentication failed.");
				return 0;
			}

			if (connlist_find_connection_by_account(a) && prefs_v3::kick_old_login() == 0) {
				std::snprintf(temp, sizeof(temp), "%s :Account is already in use!", conn_get_loggeduser(conn));
				irc_send(conn, ERR_NICKNAMEINUSE, temp);
			}
			else if (account_get_auth_lock(a) == 1) {
				message_send_text(conn, message_type_notice, NULL, "Authentication rejected (account is locked) ");
			}
			else
			{
				hash_set_str(&h1, passhash);
				temphash = account_get_pass(a);
				hash_set_str(&h2, temphash);
				if (hash_eq(h1, h2)) {
					conn_login(conn, a, username);
					conn_set_state(conn, conn_state_loggedin);
					conn_set_clienttag(conn, CLIENTTAG_IIRC_UINT); /* IIRC hope here is ok */
					message_send_text(conn, message_type_notice, NULL, "Authentication successful. You are now logged in.");
					return 1;
				}
				else {
					message_send_text(conn, message_type_notice, NULL, "Authentication failed."); /* wrong password */
					conn_increment_passfail_count(conn);
				}
			}
			return 0;
		}

		/* Channel name conversion rules: */
		/* Not allowed in IRC (RFC2812): NUL, BELL, CR, LF, ' ', ':' and ','*/
		/*   ' '  -> '_'      */
		/*   '_'  -> '%_'     */
		/*   '%'  -> '%%'     */
		/*   '\b' -> '%b'     */
		/*   '\n' -> '%n'     */
		/*   '\r' -> '%r'     */
		/*   ':'  -> '%='     */
		/*   ','  -> '%-'     */
		/* In IRC a channel can be specified by '#'+channelname or '!'+channelid */
		extern char const * irc_convert_channel(t_channel const * channel, t_connection * c)
		{
			char const * bname;
			static char out[MAX_CHANNELNAME_LEN];
			unsigned int outpos;
			int i;

			if (!channel)
				return "*";

			std::memset(out, 0, sizeof(out));
			out[0] = '#';
			outpos = 1;

			if ((conn_get_wol(c) == 1) && (channel_get_clienttag(channel) != 0 && (conn_get_clienttag(c) == channel_get_clienttag(channel))))
				bname = channel_get_shortname(channel); /* We converting unreadable "lob 18 0" names to human redable ones */
			else
				bname = channel_get_name(channel);

			for (i = 0; bname[i] != '\0'; i++) {
				if (bname[i] == ' ') {
					out[outpos++] = '_';
				}
				else if (bname[i] == '_') {
					out[outpos++] = '%';
					out[outpos++] = '_';
				}
				else if (bname[i] == '%') {
					out[outpos++] = '%';
					out[outpos++] = '%';
				}
				else if (bname[i] == '\b') {
					out[outpos++] = '%';
					out[outpos++] = 'b';
				}
				else if (bname[i] == '\n') {
					out[outpos++] = '%';
					out[outpos++] = 'n';
				}
				else if (bname[i] == '\r') {
					out[outpos++] = '%';
					out[outpos++] = 'r';
				}
				else if (bname[i] == ':') {
					out[outpos++] = '%';
					out[outpos++] = '=';
				}
				else if (bname[i] == ',') {
					out[outpos++] = '%';
					out[outpos++] = '-';
				}
				else {
					out[outpos++] = bname[i];
				}
				if ((outpos + 2) >= (sizeof(out))) {
					std::sprintf(out, "!%u", channel_get_channelid(channel));
					return out;
				}
			}
			return out;
		}

		extern char const * irc_convert_ircname(char const * pircname)
		{
			static char out[MAX_CHANNELNAME_LEN];
			unsigned int outpos;
			int special;
			int i;
			char const * ircname = pircname + 1;

			if (!ircname) {
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL ircname");
				return NULL;
			}

			outpos = 0;
			std::memset(out, 0, sizeof(out));
			special = 0;
			if (pircname[0] == '!') {
				t_channel * channel;

				channel = channellist_find_channel_bychannelid(std::atoi(ircname));
				if (channel)
					return channel_get_name(channel);
				else
					return NULL;
			}
			else if (pircname[0] != '#') {
				return NULL;
			}
			for (i = 0; ircname[i] != '\0'; i++) {
				if (ircname[i] == '_') {
					out[outpos++] = ' ';
				}
				else if (ircname[i] == '%') {
					if (special) {
						out[outpos++] = '%';
						special = 0;
					}
					else {
						special = 1;
					}
				}
				else if (special) {
					if (ircname[i] == '_') {
						out[outpos++] = '_';
					}
					else if (ircname[i] == 'b') {
						out[outpos++] = '\b';
					}
					else if (ircname[i] == 'n') {
						out[outpos++] = '\n';
					}
					else if (ircname[i] == 'r') {
						out[outpos++] = '\r';
					}
					else if (ircname[i] == '=') {
						out[outpos++] = ':';
					}
					else if (ircname[i] == '-') {
						out[outpos++] = ',';
					}
					else {
						/* maybe it's just a typo :) */
						out[outpos++] = '%';
						out[outpos++] = ircname[i];
					}
				}
				else {
					out[outpos++] = ircname[i];
				}
				if ((outpos + 2) >= (sizeof(out))) {
					return NULL;
				}
			}
			return out;
		}

		/* splits an string list into its elements */
		/* (list will be modified) */
		static char ** irc_split_elems(char * list, int separator, int ignoreblank)
		{
			int i;
			int count;
			char ** out;

			if (!list) {
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL list");
				return NULL;
			}

			for (count = 0, i = 0; list[i] != '\0'; i++) {
				if (list[i] == separator) {
					count++;
					if (ignoreblank) {
						/* ignore more than one separators "in a row" */
						while ((list[i + 1] != '\0') && (list[i] == separator)) i++;
					}
				}
			}
			count++; /* count separators -> we have one more element ... */
			/* we also need a terminating element */
			out = new char*[count + 1]{};

			out[0] = list;
			if (count > 1) {
				for (i = 1; i < count; i++) {
					out[i] = std::strchr(out[i - 1], separator);
					if (!out[i]) {
						eventlog(eventlog_level_error, __FUNCTION__, "BUG: wrong number of separators");
						delete[] out;
						return NULL;
					}
					if (ignoreblank)
					while ((*out[i] + 1) == separator) out[i]++;
					*out[i]++ = '\0';
				}
				if ((ignoreblank) && (out[count - 1]) && (*out[count - 1] == '\0')) {
					out[count - 1] = NULL; /* last element is blank */
				}
			}
			else if ((ignoreblank) && (*out[0] == '\0')) {
				out[0] = NULL; /* now we have 2 terminators ... never mind */
			}
			out[count] = NULL; /* terminating element */
			return out;
		}

		static int irc_unget_elems(char ** elems)
		{
			if (!elems) {
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL elems");
				return -1;
			}
			delete[] elems;
			return 0;
		}

		extern char ** irc_get_listelems(char * list)
		{
			return irc_split_elems(list, ',', 0);
		}

		extern int irc_unget_listelems(char ** elems)
		{
			return irc_unget_elems(elems);
		}

		extern char ** irc_get_paramelems(char * list)
		{
			return irc_split_elems(list, ' ', 1);
		}

		extern int irc_unget_paramelems(char ** elems)
		{
			return irc_unget_elems(elems);
		}

		extern char ** irc_get_ladderelems(char * list)
		{
			return irc_split_elems(list, ':', 1);
		}

		extern int irc_unget_ladderelems(char ** elems)
		{
			return irc_unget_elems(elems);
		}

		static char * irc_message_preformat(t_irc_message_from const * from, char const * command, char const * dest, char const * text)
		{
			char * myfrom;
			char const * mydest = "";
			char const * mytext = "";
			int len;
			char * msg;

			if (!command) {
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL command");
				return NULL;
			}
			if (from) {
				if ((!from->nick) || (!from->user) || (!from->host)) {
					eventlog(eventlog_level_error, __FUNCTION__, "got malformed from");
					return NULL;
				}
				myfrom = new char[std::strlen(from->nick) + 1 + std::strlen(from->user) + 1 + std::strlen(from->host) + 1]; /* nick + "!" + user + "@" + host + "\0" */
				std::sprintf(myfrom, "%s!%s@%s", from->nick, from->user, from->host);
			}
			else
				myfrom = irc_strdup(server_get_hostname());
			if (dest)
				mydest = dest;

			if (text)
				mytext = text;

			len = 1 + std::strlen(myfrom) + 1 +
				std::strlen(command) + 1 +
				std::strlen(mydest) + 1 +
				1 + std::strlen(mytext) + 1;


			msg = new char[len];

			std::sprintf(msg, ":%s\n%s\n%s\n%s", myfrom, command, mydest, mytext);
			delete[] myfrom;
			return msg;
		}

		extern int irc_message_postformat(t_packet * packet, t_connection const * dest)
		{
			/* the four elements */
			char * e1;
			char * e1_2;
			char * e2;
			char * e3;
			char * e4;
			char const * tname = NULL;
			char const * toname = "AUTH"; /* fallback name */
			const char * temp;

			if (!packet) {
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL packet");
				return -1;
			}
			if (!dest) {
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL dest");
				return -1;
			}

			e1 = (char*)packet_get_raw_data(packet, 0);
			e2 = std::strchr(e1, '\n');
			if (!e2) {
				eventlog(eventlog_level_warn, __FUNCTION__, "malformed message (e2 missing)");
				return -1;
			}
			*e2++ = '\0';
			e3 = std::strchr(e2, '\n');
			if (!e3) {
				eventlog(eventlog_level_warn, __FUNCTION__, "malformed message (e3 missing)");
				return -1;
			}
			*e3++ = '\0';
			e4 = std::strchr(e3, '\n');
			if (!e4) {
				eventlog(eventlog_level_warn, __FUNCTION__, "malformed message (e4 missing)");
				return -1;
			}
			*e4++ = '\0';

			if (prefs_v3::hide_addr() && !(account_get_command_groups(conn_get_account(dest)) & command_get_group("/admin-addr")))
			{
				e1_2 = std::strchr(e1, '@');
				if (e1_2)
				{
					*e1_2++ = '\0';
				}
			}
			else
				e1_2 = NULL;

			if (e3[0] == '\0') { /* fill in recipient */
				if ((tname = conn_get_loggeduser(dest)))
					toname = tname;
			}
			else
				toname = e3;

			if (std::strcmp(toname, "\r") == 0) {
				toname = ""; /* HACK: the target field is really empty */
				temp = "";
			}
			else
				temp = " ";

			if (std::strlen(e1) + 1 + std::strlen(e2) + 1 + std::strlen(toname) + std::strlen(temp) + std::strlen(e4) + 2 + 1 <= MAX_IRC_MESSAGE_LEN) {
				char msg[MAX_IRC_MESSAGE_LEN + 1];

				if (e1_2)
					std::sprintf(msg, "%s@hidden %s %s%s%s", e1, e2, toname, temp, e4);
				else
					std::sprintf(msg, "%s %s %s%s%s", e1, e2, toname, temp, e4);

				DEBUG2("[{}] sent \"{}\"", conn_get_socket(dest), msg);
				std::strcat(msg, "\r\n");

				packet_set_size(packet, 0);
				packet_append_data(packet, msg, std::strlen(msg));
				if (tname)
					conn_unget_chatname(dest, tname);
				return 0;
			}
			else {
				/* FIXME: split up message? */
				eventlog(eventlog_level_warn, __FUNCTION__, "maximum IRC message length exceeded");
				if (tname)
					conn_unget_chatname(dest, tname);
				return -1;
			}
		}

		extern int irc_message_format(t_packet * packet, t_message_type type, t_connection * me, t_connection * dst, char const * text, unsigned int dstflags)
		{
			char * msg;
			char const * ctag;
			t_irc_message_from from;

			if (!packet)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL packet");
				return -1;
			}

			msg = NULL;
			if (me)
				ctag = clienttag_uint_to_str(conn_get_clienttag(me));
			else
				ctag = clienttag_uint_to_str(CLIENTTAG_IIRC_UINT);

			switch (type)
			{
				/* case message_type_adduser: this is sent manually in handle_irc */
			case message_type_adduser:
				/* when we do it somewhere else, then we can also make sure to not get our logs spammed */
				break;
			case message_type_join:
			{
									  t_channel * channel;
									  from.nick = conn_get_chatname(me);
									  from.user = ctag;
									  from.host = addr_num_to_ip_str(conn_get_addr(me));

									  channel = conn_get_channel(me);

									  if (tag_check_wolv2(conn_get_clienttag(dst))) {
										  char temp[MAX_IRC_MESSAGE_LEN];
										  t_clan * clan;
										  unsigned int clanid = 0;
										  std::memset(temp, 0, sizeof(temp));

										  /**
										  *  For WOLv2 the channel JOIN output must be like the following:
										  *  user!WWOL@hostname JOIN :clanID,longIP channelName
										  */

										  clan = account_get_clan(conn_get_account(me));

										  if (clan)
											  clanid = clan_get_clanid(clan);

										  std::sprintf(temp, ":%u,%u", clanid, conn_get_addr(me));
										  msg = irc_message_preformat(&from, "JOIN", temp, irc_convert_channel(channel, dst));
									  }
									  else {
										  msg = irc_message_preformat(&from, "JOIN", "\r", irc_convert_channel(channel, dst));
									  }
									  conn_unget_chatname(me, from.nick);
			}
				break;
			case message_type_part:
				from.nick = conn_get_chatname(me);
				from.user = ctag;
				from.host = addr_num_to_ip_str(conn_get_addr(me));
				msg = irc_message_preformat(&from, "PART", "\r", irc_convert_channel(conn_get_channel(me), dst));
				conn_unget_chatname(me, from.nick);
				break;
			case message_type_talk:
			case message_type_whisper:
			{
										 char const * dest;
										 char temp[MAX_IRC_MESSAGE_LEN];

										 if (me) {
											 from.nick = conn_get_chatname(me);
											 from.host = addr_num_to_ip_str(conn_get_addr(me));
										 }
										 else {
											 from.nick = server_get_hostname();
											 from.host = server_get_hostname();
										 }
										 from.user = ctag;

										 if (type == message_type_talk)
											 dest = irc_convert_channel(conn_get_channel(me), dst); /* FIXME: support more channels and choose right one! */
										 else
											 dest = ""; /* will be replaced with username in postformat */

										 std::sprintf(temp, ":%s", text);

										 if ((conn_get_wol(me) != 1) && (conn_get_wol(dst) == 1) && (conn_wol_get_pageme(dst))
											 && (type == message_type_whisper) && (conn_get_channel(me) != (conn_get_channel(dst))))
											 msg = irc_message_preformat(&from, "PAGE", NULL, temp);
										 else
											 msg = irc_message_preformat(&from, "PRIVMSG", dest, temp);

										 if (me)
											 conn_unget_chatname(me, from.nick);
			}
				break;
			case message_type_emote:
			{
									   char const * dest;
									   char temp[MAX_IRC_MESSAGE_LEN];

									   /* "\001ACTION " + text + "\001" + \0 */

									   /* PELISH: WOLv1, DUNE2000 and RENEGADE shows emotes automaticaly to self */
									   if ((me == dst) && ((tag_check_wolv1(conn_get_clienttag(dst))) ||
										   (conn_get_clienttag(dst) == CLIENTTAG_DUNE2000_UINT) ||
										   (conn_get_clienttag(dst) == CLIENTTAG_RENEGADE_UINT) ||
										   (conn_get_clienttag(dst) == CLIENTTAG_RENGDFDS_UINT)))
										   break;

									   if ((8 + std::strlen(text) + 1 + 1) <= MAX_IRC_MESSAGE_LEN) {
										   std::sprintf(temp, ":\001ACTION %s\001", text);
									   }
									   else {
										   std::sprintf(temp, ":\001ACTION (maximum message length exceeded)\001");
									   }
									   from.nick = conn_get_chatname(me);
									   from.user = ctag;
									   from.host = addr_num_to_ip_str(conn_get_addr(me));
									   /* FIXME: also supports whisper emotes? */
									   dest = irc_convert_channel(conn_get_channel(me), dst); /* FIXME: support more channels and choose right one! */
									   msg = irc_message_preformat(&from, "PRIVMSG", dest, temp);
									   conn_unget_chatname(me, from.nick);
			}
				break;
			case message_type_broadcast:
			case message_type_info:
			case message_type_error:
			{
									   char temp[MAX_IRC_MESSAGE_LEN];
									   std::sprintf(temp, ":%s", text);
									   if (conn_get_wol(dst) == 1) {
										   if ((type == message_type_info) || (type == message_type_error))
											   msg = irc_message_preformat(NULL, "PAGE", NULL, temp);
										   else
											   msg = irc_message_preformat(NULL, "NOTICE", NULL, temp);
									   }
									   else
										   msg = irc_message_preformat(NULL, "NOTICE", NULL, temp);
			}
				break;
			case message_type_channel:
				/* ignore it */
				break;
			case message_type_userflags:
				/* ignore it but maybe will be used for set MODE +o if user is operator
				 * but at this time is this command sended only once when user join
				 * first channel */
				break;
			case message_type_mode:
				from.nick = conn_get_chatname(me);
				from.user = ctag;
				from.host = addr_num_to_ip_str(conn_get_addr(me));
				msg = irc_message_preformat(&from, "MODE", "\r", text);
				conn_unget_chatname(me, from.nick);
				break;
			case message_type_nick:
			{
									  from.nick = conn_get_loggeduser(me);
									  from.host = addr_num_to_ip_str(conn_get_addr(me));
									  from.user = ctag;
									  msg = irc_message_preformat(&from, "NICK", "\r", text);
			}
				break;
			case message_type_notice:
			{
										char temp[MAX_IRC_MESSAGE_LEN];
										std::sprintf(temp, ":%s", text);

										if (me && conn_get_chatname(me))
										{
											from.nick = conn_get_chatname(me);
											from.host = addr_num_to_ip_str(conn_get_addr(me));
											from.user = ctag;
											msg = irc_message_preformat(&from, "NOTICE", "", temp);
										}
										else
										{
											msg = irc_message_preformat(NULL, "NOTICE", "", temp);
										}
			}
				break;
			case message_type_namreply:
			{
										  t_channel * channel;

										  channel = conn_get_channel(me);

										  irc_send_rpl_namreply(dst, channel);
			}
				break;
			case message_type_topic:
			{
									   t_channel * channel;
									   channel = conn_get_channel(me);
									   irc_send_topic(dst, channel);
			}
				break;
			case message_type_kick:
			{
									  char temp[MAX_IRC_MESSAGE_LEN];
									  from.nick = conn_get_chatname(me);
									  from.user = ctag;
									  from.host = addr_num_to_ip_str(conn_get_addr(me));
									  if (text)
										  std::sprintf(temp, "%s :%s", conn_get_chatname(me), text);
									  else
										  std::sprintf(temp, "%s :", conn_get_chatname(me));
									  msg = irc_message_preformat(&from, "KICK", irc_convert_channel(conn_get_channel(me), dst), temp);
									  conn_unget_chatname(me, from.nick);
			}
				break;
			case message_type_quit:
				if (conn_get_clienttag(dst) == CLIENTTAG_IIRC_UINT) {
					char temp[MAX_IRC_MESSAGE_LEN];
					if (text)
						sprintf(temp, ":%s", text);
					else
						sprintf(temp, ":");

					from.nick = conn_get_chatname(me);
					from.host = addr_num_to_ip_str(conn_get_addr(me));
					from.user = ctag;
					msg = irc_message_preformat(&from, "QUIT", "\r", temp);
				}
				else {
					from.nick = conn_get_chatname(me);
					from.user = ctag;
					from.host = addr_num_to_ip_str(conn_get_addr(me));
					msg = irc_message_preformat(&from, "PART", "\r", irc_convert_channel(conn_get_channel(me), dst));
					conn_unget_chatname(me, from.nick);
				}
				break;

				/**
				*  Westwood Online Extensions
				*/
			case message_type_host:
				from.nick = conn_get_chatname(me);
				from.user = ctag;
				from.host = addr_num_to_ip_str(conn_get_addr(me));
				msg = irc_message_preformat(&from, "HOST", "\r", text);
				conn_unget_chatname(me, from.nick);
				break;
			case message_type_invmsg:
				from.nick = conn_get_chatname(me);
				from.user = ctag;
				from.host = addr_num_to_ip_str(conn_get_addr(me));
				msg = irc_message_preformat(&from, "INVMSG", NULL, text);
				conn_unget_chatname(me, from.nick);
				break;
			case message_type_page:
			{
									  char temp[MAX_IRC_MESSAGE_LEN];
									  from.nick = conn_get_chatname(me);
									  from.user = ctag;
									  from.host = addr_num_to_ip_str(conn_get_addr(me));
									  std::sprintf(temp, ":%s", text);
									  msg = irc_message_preformat(&from, "PAGE", NULL, temp);
									  conn_unget_chatname(me, from.nick);
			}
				break;
			case message_wol_joingame:
				from.nick = conn_get_chatname(me);
				from.user = ctag;
				from.host = addr_num_to_ip_str(conn_get_addr(me));
				msg = irc_message_preformat(&from, "JOINGAME", "\r", text);
				conn_unget_chatname(me, from.nick);
				break;
			case message_type_gameopt_talk:
			case message_type_gameopt_whisper:
			{
												 char const * dest;
												 char temp[MAX_IRC_MESSAGE_LEN];

												 if (me) {
													 from.nick = conn_get_chatname(me);
													 from.host = addr_num_to_ip_str(conn_get_addr(me));
												 }
												 else {
													 from.nick = server_get_hostname();
													 from.host = server_get_hostname();
												 }
												 from.user = ctag;

												 if (type == message_type_gameopt_talk)
													 dest = irc_convert_channel(conn_get_channel(me), dst); /* FIXME: support more channels and choose right one! */
												 else
													 dest = ""; /* will be replaced with username in postformat */

												 std::sprintf(temp, ":%s", text);

												 msg = irc_message_preformat(&from, "GAMEOPT", dest, temp);

												 if (me)
													 conn_unget_chatname(me, from.nick);
			}
				break;
			case message_wol_start_game:
				from.nick = conn_get_chatname(me);
				from.user = ctag;
				from.host = addr_num_to_ip_str(conn_get_addr(me));
				msg = irc_message_preformat(&from, "STARTG", NULL, text);
				conn_unget_chatname(me, from.nick);
				break;
			case message_wol_advertr:
				msg = irc_message_preformat(NULL, "ADVERTR", "\r", text);
				break;
			case message_wol_chanchk:
				msg = irc_message_preformat(NULL, "CHANCHK", "\r", text);
				break;
			case message_wol_userip:
				from.nick = conn_get_chatname(me);
				from.user = ctag;
				from.host = addr_num_to_ip_str(conn_get_addr(me));
				msg = irc_message_preformat(&from, "USERIP", "\r", text);
				conn_unget_chatname(me, from.nick);
				break;
			default:
				eventlog(eventlog_level_warn, __FUNCTION__, "{} not yet implemented", type);
				return -1;
			}

			if (msg) {
				packet_append_string(packet, msg);
				delete[] msg;
				return 0;
			}
			return -1;
		}

		int irc_send_rpl_namreply_internal(t_connection * c, t_channel const * channel){
			char temp[MAX_IRC_MESSAGE_LEN];
			char const * ircname;
			int first = 1;
			t_connection * m;

			if (!channel) {
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL channel");
				return -1;
			}

			std::memset(temp, 0, sizeof(temp));
			ircname = irc_convert_channel(channel, c);
			if (!ircname) {
				eventlog(eventlog_level_error, __FUNCTION__, "channel has NULL ircname");
				return -1;
			}
			/* '@' = secret; '*' = private; '=' = public */
			if ((1 + 1 + std::strlen(ircname) + 2 + 1) <= MAX_IRC_MESSAGE_LEN) {
				std::sprintf(temp, "%c %s :", ((channel_get_permanent(channel)) ? ('=') : ('*')), ircname);
			}
			else {
				eventlog(eventlog_level_warn, __FUNCTION__, "maximum message length exceeded");
				return -1;
			}

			if (!(channel_get_flags(channel) & channel_flags_thevoid))
			for (m = channel_get_first(channel); m; m = channel_get_next()) {
				char const * name = conn_get_chatname(m);
				char flg[5] = "";
				unsigned int flags;

				if (!name)
					continue;
				flags = conn_get_flags(m);
				if (flags & MF_BLIZZARD)
					std::strcat(flg, "@");
				else if ((flags & MF_BNET) || (flags & MF_GAVEL))
					std::strcat(flg, "%");
				else if (flags & MF_VOICE)
					std::strcat(flg, "+");
				if ((std::strlen(temp) + ((!first) ? (1) : (0)) + std::strlen(flg) + std::strlen(name) + 1) <= sizeof(temp)) {
					if (!first) std::strcat(temp, " ");
					if (conn_get_wol(c) == 1) {
						char _temp[MAX_IRC_MESSAGE_LEN];
						t_game * game = conn_get_game(m);
						if ((first) && ((std::strcmp(ircname, "#Lob_38_0") == 0) ||
							(std::strcmp(ircname, "#Lob_39_0") == 0) ||
							(std::strcmp(ircname, "#Lob_40_0") == 0))) {

							std::sprintf(_temp, "@matchbot,0,0 ");
							std::strcat(temp, _temp);
							first = 0;

						}

						if ((game) && (game_get_channel(game) == channel)) {
							if (game_get_owner(game) == m) {
								/* PELISH: Only game owners will have OP flag (this prevent official OP to be normal player) */
								std::strcat(temp, "@");
							}
						}
						else {
							if ((flags & MF_BNET) || (flags & MF_GAVEL))
								std::sprintf(flg, "@");  /* PELISH: WOL does not understand to '%' char so we using '@' for TempOP too */
							std::strcat(temp, flg);
						}
						if (tag_check_wolv2(conn_get_clienttag(c))) {
							/* BATTLECLAN Support */
							std::memset(_temp, 0, sizeof(_temp));
							t_clan * clan = account_get_clan(conn_get_account(m));
							unsigned int clanid = 0;

							if (clan)
								clanid = clan_get_clanid(clan);

							std::sprintf(_temp, "%s,%u,%u", name, clanid, conn_get_addr(m));
							std::strcat(temp, _temp);
						}
						else {
							std::strcat(temp, name);
						}
					}
					else {
						std::strcat(temp, flg);
						std::strcat(temp, name);
					}

					first = 0;
				}
				conn_unget_chatname(m, name);
			}
			irc_send(c, RPL_NAMREPLY, temp);

			return 0;
		}

		extern int irc_send_rpl_namreply(t_connection * c, t_channel const * channel)
		{
			char temp[MAX_IRC_MESSAGE_LEN];
			char const * ircname;

			if (!c) {
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL connection");
				return -1;
			}

			if (channel) {
				ircname = irc_convert_channel(channel, c);
				if (!ircname) {
					eventlog(eventlog_level_error, __FUNCTION__, "channel has NULL ircname");
					return -1;
				}
				irc_send_rpl_namreply_internal(c, channel);
				std::sprintf(temp, "%.32s :End of NAMES list", ircname);
			}
			else {
				for (t_channel const* ch : channellist())
				{
					irc_send_rpl_namreply_internal(c, ch);
				}
				std::sprintf(temp, "* :End of NAMES list");
			}

			irc_send(c, RPL_ENDOFNAMES, temp);
			return 0;
		}

		static int irc_who_connection(t_connection * dest, t_connection * c)
		{
			t_account * a;
			char const * tempuser;
			char const * tempowner;
			char const * tempname;
			char const * tempip;
			char const * tempflags = "@"; /* FIXME: that's dumb */
			char temp[MAX_IRC_MESSAGE_LEN];
			char const * tempchannel;

			if (!dest) {
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL destination");
				return -1;
			}
			if (!c) {
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL connection");
				return -1;
			}
			a = conn_get_account(c);
			if (!(tempuser = clienttag_uint_to_str(conn_get_clienttag(c))))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL clienttag (tempuser)");
				return -1;
			}
			if (!(tempowner = account_get_ll_owner(a)))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL ll_owner (tempowner)");
				return -1;
			}
			if (!(tempname = conn_get_username(c)))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL username (tempname)");
				return -1;
			}
			if (!(tempip = addr_num_to_ip_str(conn_get_addr(c))))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL addr (tempip)");
				return -1;
			}
			if (!(tempchannel = irc_convert_channel(conn_get_channel(c), c)))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL channel (tempchannel)");
				return -1;
			}
			if ((std::strlen(tempchannel) + 1 + std::strlen(tempuser) + 1 + std::strlen(tempip) + 1 + std::strlen(server_get_hostname()) + 1 + std::strlen(tempname) + 1 + 1 + std::strlen(tempflags) + 4 + std::strlen(tempowner) + 1) > MAX_IRC_MESSAGE_LEN) {
				eventlog(eventlog_level_info, __FUNCTION__, "WHO reply too long - skip");
				return -1;
			}
			else
				std::sprintf(temp, "%s %s %s %s %s %c%s :0 %s", tempchannel, tempuser, tempip, server_get_hostname(), tempname, 'H', tempflags, tempowner);
			irc_send(dest, RPL_WHOREPLY, temp);
			return 0;
		}

		extern int irc_who(t_connection * c, char const * name)
		{
			/* FIXME: support wildcards! */

			if (!c) {
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL connection");
				return -1;
			}
			if (!name) {
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL name");
				return -1;
			}
			if ((name[0] == '#') || (name[0] == '&') || (name[0] == '!')) {
				/* it's a channel */
				t_connection * info;
				t_channel * channel;
				char const * ircname;

				ircname = irc_convert_ircname(name);
				channel = channellist_find_channel_by_name(ircname, NULL, NULL);
				if (!channel) {
					char temp[MAX_IRC_MESSAGE_LEN];

					if ((std::strlen(":No such channel") + 1 + std::strlen(name) + 1) <= MAX_IRC_MESSAGE_LEN) {
						std::sprintf(temp, ":No such channel %s", name);
						irc_send(c, ERR_NOSUCHCHANNEL, temp);
					}
					else {
						irc_send(c, ERR_NOSUCHCHANNEL, ":No such channel");
					}
					return 0;
				}
				for (info = channel_get_first(channel); info; info = channel_get_next()) {
					irc_who_connection(c, info);
				}
			}
			else {
				/* it's just one user */
				t_connection * info;

				if ((info = connlist_find_connection_by_accountname(name)))
					return irc_who_connection(c, info);
			}
			return 0;
		}

		extern int irc_send_motd(t_connection * conn)
		{
			char const * tempname;
			char const * filename;
			std::FILE *fp;
			char * line, *formatted_line;
			char send_line[MAX_IRC_MESSAGE_LEN];
			char motd_failed = 0;
			bool first = true;

			if (!conn) {
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL connection");
				return -1;
			}

			tempname = conn_get_loggeduser(conn);

			if ((filename = prefs_v3::motdfile())) {
				if ((fp = std::fopen(filename, "r"))) {
					while ((line = file_get_line(fp))) {
						if ((formatted_line = message_format_line(conn, line))) {
							formatted_line[0] = ' ';
							std::sprintf(send_line, ":-%s", formatted_line);
							if (first) {
								irc_send(conn, RPL_MOTDSTART, send_line);
								first = false;
							}
							else
								irc_send(conn, RPL_MOTD, send_line);
							delete[] formatted_line;
						}
					}

					file_get_line(NULL); // clear file_get_line buffer
					std::fclose(fp);
				}
				else
					motd_failed = 1;
			}
			else
				motd_failed = 1;

			if (motd_failed) {
				irc_send(conn, RPL_MOTDSTART, ":- Failed to load motd, sending default motd              ");
				irc_send(conn, RPL_MOTD, ":- ====================================================== ");
				irc_send(conn, RPL_MOTD, ":-                 http://www.pvpgn.pro                   ");
				irc_send(conn, RPL_MOTD, ":- ====================================================== ");
			}
			irc_send(conn, RPL_ENDOFMOTD, ":End of /MOTD command");
			return 0;
		}

		int irc_welcome(t_connection * conn){
			if (conn_get_wol(conn)) {
				handle_wol_welcome(conn);
				return 0;
			}

			// TODO(Phase3): handled by v3 IrcFsm — legacy welcome inlined from handle_irc.cpp
			char temp[MAX_IRC_MESSAGE_LEN];
			std::time_t temptime;
			char const * tempname;
			char const * temptimestr;

			if (!conn) {
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL connection");
				return -1;
			}

			tempname = conn_get_loggeduser(conn);

			if ((34 + std::strlen(tempname) + 1) <= MAX_IRC_MESSAGE_LEN)
				std::sprintf(temp, ":Welcome to the %s IRC Network %s", prefs_v3::irc_network_name(), tempname);
			else
				std::sprintf(temp, ":Maximum length exceeded");
			irc_send(conn, RPL_WELCOME, temp);

			if ((14 + std::strlen(server_get_hostname()) + 10 + std::strlen(PVPGN_SOFTWARE " " PVPGN_VERSION) + 1) <= MAX_IRC_MESSAGE_LEN)
				std::sprintf(temp, ":Your host is %s, running " PVPGN_SOFTWARE " " PVPGN_VERSION, server_get_hostname());
			else
				std::sprintf(temp, ":Maximum length exceeded");
			irc_send(conn, RPL_YOURHOST, temp);

			temptime = server_get_starttime(); /* FIXME: This should be build time */
			temptimestr = std::ctime(&temptime);
			if ((25 + std::strlen(temptimestr) + 1) <= MAX_IRC_MESSAGE_LEN)
				std::sprintf(temp, ":This server was created %s", temptimestr); /* FIXME: is ctime() portable? */
			else
				std::sprintf(temp, ":Maximum length exceeded");
			irc_send(conn, RPL_CREATED, temp);

			/* we don't give mode information on MYINFO we give it on ISUPPORT */
			if ((std::strlen(server_get_hostname()) + 7 + std::strlen(PVPGN_SOFTWARE " " PVPGN_VERSION) + 9 + 1) <= MAX_IRC_MESSAGE_LEN)
				std::sprintf(temp, "%s " PVPGN_SOFTWARE " " PVPGN_VERSION " - -", server_get_hostname());
			else
				std::sprintf(temp, ":Maximum length exceeded");
			irc_send(conn, RPL_MYINFO, temp);

			std::sprintf(temp, "NICKLEN=%d TOPICLEN=%d CHANNELLEN=%d PREFIX=%s CHANTYPES=" CHANNEL_TYPE " NETWORK=%s IRCD=" PVPGN_SOFTWARE,
				MAX_CHARNAME_LEN, MAX_TOPIC_LEN, MAX_CHANNELNAME_LEN, CHANNEL_PREFIX, prefs_v3::irc_network_name());

			irc_send(conn, RPL_ISUPPORT, temp);

			irc_send_motd(conn);

			message_send_text(conn, message_type_notice, NULL, "This is an experimental service");

			conn_set_state(conn, conn_state_bot_password);

			if (conn_get_ircpass(conn)) {
				message_send_text(conn, message_type_notice, NULL, "Trying to authenticate with PASS ...");
				irc_authenticate(conn, conn_get_ircpass(conn));
			}
			else {
				message_send_text(conn, message_type_notice, NULL, "No PASS command received. Please identify yourself by /msg NICKSERV identify <password>.");
			}
			return 0;
		}

		extern int _handle_nick_command(t_connection * conn, int numparams, char ** params, char * text)
		{
			char temp[MAX_IRC_MESSAGE_LEN];
			/* FIXME: more strict param checking */

			if ((conn_get_loggeduser(conn)) &&
				(conn_get_state(conn) != conn_state_bot_password &&
				conn_get_state(conn) != conn_state_bot_username)) {
				irc_send(conn, ERR_RESTRICTED, ":You can't change your nick after login");
			}
			else {
				if ((params) && (params[0])) {
					if (conn_get_loggeduser(conn)){
						std::snprintf(temp, sizeof(temp), ":%s", params[0]);
						message_send_text(conn, message_type_nick, conn, temp);
					}
					conn_set_loggeduser(conn, params[0]);
				}
				else if (text) {
					if (conn_get_loggeduser(conn)){
						std::snprintf(temp, sizeof(temp), ":%s", text);
						message_send_text(conn, message_type_nick, conn, temp);
					}
					conn_set_loggeduser(conn, text);
				}
				else
					irc_send(conn, ERR_NEEDMOREPARAMS, "NICK :Not enough parameters");

				if ((conn_get_user(conn)) && (conn_get_loggeduser(conn)))
					irc_welcome(conn); /* only send the welcome if we have USER and NICK */
			}
			return 0;
		}

		extern int _handle_ping_command(t_connection * conn, int numparams, char ** params, char * text)
		{
			/* Dizzy: just ignore this because RFC says we should not reply client PINGs
			 * NOTE: RFC2812 doesn't seem to be very expressive about this ... */
			if (numparams)
				irc_send_pong(conn, params[0]);
			else
				irc_send_pong(conn, text);
			return 0;
		}

		extern int _handle_pong_command(t_connection * conn, int numparams, char ** params, char * text)
		{
			/* NOTE: RFC2812 doesn't seem to be very expressive about this ... */
			if (conn_get_ircping(conn) == 0) {
				eventlog(eventlog_level_warn, __FUNCTION__, "[{}] PONG without PING", conn_get_socket(conn));
			}
			else {
				unsigned int val = 0;
				char * sname;

				if (numparams >= 1) {
					val = std::strtoul(params[0], NULL, 10);
					sname = params[0];
				}
				else if (text) {
					val = std::strtoul(text, NULL, 10);
					sname = text;
				}
				else {
					val = 0;
					sname = 0;
				}

				if (conn_get_ircping(conn) != val) {
					if ((!(sname)) || (std::strcmp(sname, server_get_hostname()) != 0)) {
						/* Actually the servername should not be always accepted but we aren't that pedantic :) */
						eventlog(eventlog_level_warn, __FUNCTION__, "[{}] got bad PONG ({}!={} && {}!={})", conn_get_socket(conn), val, conn_get_ircping(conn), sname, server_get_hostname());
						return -1;
					}
				}
				conn_set_latency(conn, get_ticks() - conn_get_ircping(conn));
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] latency is now {} ({}-{})", conn_get_socket(conn), get_ticks() - conn_get_ircping(conn), get_ticks(), conn_get_ircping(conn));
				conn_set_ircping(conn, 0);
			}
			return 0;
		}

		extern int _handle_join_command(t_connection * conn, int numparams, char ** params, char * text)
		{
			if (numparams >= 1) {
				char ** e;

				/* According to RFC2812 - if channelname is "0" we need to PART all channels */
				if (std::strcmp(params[0], "0") == 0) {
					conn_part_channel(conn);
					return 0;
				}

				e = irc_get_listelems(params[0]);
				if ((e) && (e[0])) {
					char temp[MAX_IRC_MESSAGE_LEN];
					char const * ircname = irc_convert_ircname(e[0]);
					t_channel * old_channel = conn_get_channel(conn);
					t_channel * channel;
					t_account * acc = conn_get_account(conn);

					if ((ircname) && (channel = channellist_find_channel_by_name(ircname, NULL, NULL))) {
						if (channel_check_banning(channel, conn)) {
							std::snprintf(temp, sizeof(temp), "%s :You are banned from that channel.", e[0]);
							irc_send(conn, ERR_BANNEDFROMCHAN, temp);
							if (e)
								irc_unget_listelems(e);
							return 0;
						}

						if ((account_get_auth_admin(acc, NULL) != 1) && (account_get_auth_admin(acc, ircname) != 1) &&
							(account_get_auth_operator(acc, NULL) != 1) && (account_get_auth_operator(acc, ircname) != 1) &&
							(channel_get_max(channel) != -1) && (channel_get_curr(channel) >= channel_get_max(channel))) {

							std::snprintf(temp, sizeof(temp), "%s :The channel is currently full.", e[0]);
							irc_send(conn, ERR_CHANNELISFULL, temp);
							if (e)
								irc_unget_listelems(e);
							return 0;
						}
					}

					if ((!(ircname)) || (conn_set_channel(conn, ircname) < 0)) {
						std::snprintf(temp, sizeof(temp), "%s :JOIN failed", e[0]);
						irc_send(conn, ERR_NOSUCHCHANNEL, temp); /* Anything is still bad */
					}
				}
				if (e)
					irc_unget_listelems(e);
			}
			else
				irc_send(conn, ERR_NEEDMOREPARAMS, "JOIN :Not enough parameters");
			return 0;
		}

		extern int irc_send_topic(t_connection * c, t_channel const * channel)
		{
			class_topic Topic;
			std::string topicstr = Topic.get(channel_get_name(channel));
			char temp[MAX_IRC_MESSAGE_LEN];

			if (topicstr.empty() == false) {
				std::snprintf(temp, sizeof(temp), "%s :%s", irc_convert_channel(channel, c), topicstr.c_str());
				irc_send(c, RPL_TOPIC, temp);
			}
			else {
				std::snprintf(temp, sizeof(temp), "%s :", irc_convert_channel(channel, c));
				irc_send(c, RPL_TOPIC, temp);
				/* PELISH: This is according to RFC2812 but brakes WOLv1/WOLv2 */
				//snprintf(temp, sizeof(temp), "%s :No topic is set", irc_convert_channel(channel,c));
				//irc_send(c, RPL_NOTOPIC, temp);
			}
			return 0;
		}

		extern int _handle_topic_command(t_connection * conn, int numparams, char ** params, char * text)
		{
			char ** e = NULL;
			char temp[MAX_IRC_MESSAGE_LEN];
			class_topic Topic;

			if (params && params[0])
			{
				if (conn_get_wol(conn) == 1) {
					t_channel * channel = conn_get_channel(conn);
					if (channel)
						Topic.set(std::string(channel_get_name(channel)), std::string(text), false);
					else {
						std::snprintf(temp, sizeof(temp), "%s :You're not on that channel", params[0]);
						irc_send(conn, ERR_NOTONCHANNEL, temp);
					}
				}
				e = irc_get_listelems(params[0]);
			}
			if ((e) && (e[0])) 
			{
				t_channel *channel = conn_get_channel(conn);

				if (channel) {
					char const * ircname = irc_convert_ircname(e[0]);

					if ((ircname) && (strcasecmp(channel_get_name(channel), ircname) == 0)) {
						irc_send_topic(conn, channel);
					}
					else {
						std::snprintf(temp, sizeof(temp), "%s :You're not on that channel", e[0]);
						irc_send(conn, ERR_NOTONCHANNEL, temp);
					}
				}
				else {
					std::snprintf(temp, sizeof(temp), "%s :You're not on that channel", e[0]);
					irc_send(conn, ERR_NOTONCHANNEL, temp);
				}
			}
			else
				irc_send(conn, ERR_NEEDMOREPARAMS, "TOPIC :Not enough parameters");

			if (e)
			{
				irc_unget_listelems(e);
			}

			return 0;
		}

		extern int _handle_kick_command(t_connection * conn, int numparams, char ** params, char * text)
		{
			char temp[MAX_IRC_MESSAGE_LEN];
			char ** e;
			/**
			*  Heres the imput expected
			*  KICK [channel] [kicked_user],[kicked_user2]
			*
			*  Heres the output expected
			*  :user!WWOL@hostname KICK [channel] [kicked_user] :[text]
			*
			*  WOL in [text] sends Admin name
			*/

			if ((numparams != 2) || !(params[1])) {
				irc_send(conn, ERR_NEEDMOREPARAMS, "KICK :Not enough parameters");
				return 0;
			}

			if (e = irc_get_listelems(params[1]))
			{
				/* Make standart PvPGN KICK from RFC2812 KICK */
				if (text)
					std::snprintf(temp, sizeof(temp), "/kick %s %s", e[0], text);
				else
					std::snprintf(temp, sizeof(temp), "/kick %s", e[0]);

				handle_command(conn, temp);

				irc_unget_listelems(e);
			}
			return 0;
		}

		static int irc_send_banlist(t_connection * conn, t_channel * channel)
		{
			char const * ircname = server_get_hostname();
			char temp[MAX_IRC_MESSAGE_LEN];

			if (!conn) {
				ERROR0("got NULL conn");
				return -1;
			}

			if (!channel) {
				ERROR0("got NULL channel");
				return -1;
			}

			for (const auto& banned : channel_get_banlist(channel)) {
				//FIXME: right now we lie about who have gives ban and also about bantime
				std::snprintf(temp, sizeof(temp), "%s %s!*@* %s 1208297879", irc_convert_channel(channel, conn), banned.c_str(), ircname);
				irc_send(conn, RPL_BANLIST, temp);
			}
			return 0;
		}

		extern int _handle_mode_command(t_connection * conn, int numparams, char ** params, char * text)
		{
			char temp[MAX_IRC_MESSAGE_LEN];
			t_account * acc = conn_get_account(conn);

			std::memset(temp, 0, sizeof(temp));

			if (numparams < 1) {
				irc_send(conn, ERR_NEEDMOREPARAMS, "MODE :Not enough parameters");
				return 0;
			}

			if (params[0][0] == '#') {
				/* Channel mode */
				t_channel * channel;
				char const * ircname = irc_convert_ircname(params[0]);

				/* FIXME: Supports more than one channel in MODE command */
				if (!(channel = conn_get_channel(conn))) {
					std::snprintf(temp, sizeof(temp), "%s :No such channel", params[0]);
					irc_send(conn, ERR_NOSUCHCHANNEL, temp);
					return 0;
				}

				if (numparams == 1) {
					/* FIXME: Send real CHANELMODE flags! */
					std::snprintf(temp, sizeof(temp), "%s +tns", params[0]);
					irc_send(conn, RPL_CHANNELMODEIS, temp);
					return 0;
				}

				if (numparams == 2) {
					if (std::strcmp(params[1], "b") == 0) {
						irc_send_banlist(conn, channel);
						std::snprintf(temp, sizeof(temp), "%s :End of channel ban list", params[0]);
						irc_send(conn, RPL_ENDOFBANLIST, temp);
						return 0;
					}
					else {
						std::snprintf(temp, sizeof(temp), ":%s is unknown mode char to me for %s", params[1], params[0]);
						irc_send(conn, ERR_UNKNOWNMODE, temp);
						return 0;
					}
				}

				/* PELISH: Also tmpOP have setting modes alowed because all new channels have only tmpOP */
				if ((channel_conn_is_tmpOP(channel, conn) != 1) &&
					(account_get_auth_admin(acc, NULL) != 1) && (account_get_auth_admin(acc, ircname) != 1) &&
					(account_get_auth_operator(acc, NULL) != 1) && (account_get_auth_operator(acc, ircname) != 1)) {
					std::snprintf(temp, sizeof(temp), "%s :You're not channel operator", params[0]);
					irc_send(conn, ERR_CHANOPRIVSNEEDED, temp);
					return 0;
				}

				if (std::strcmp(params[1], "+b") == 0) {
					channel_ban_user(channel, params[2]);
					std::snprintf(temp, sizeof(temp), "%s %s %s!*@*", params[0], params[1], params[2]);
					channel_message_send(channel, message_type_mode, conn, temp);
				}
				else if (std::strcmp(params[1], "-b") == 0) {
					channel_unban_user(channel, params[2]);
					std::snprintf(temp, sizeof(temp), "%s %s %s!*@*", params[0], params[1], params[2]);
					channel_message_send(channel, message_type_mode, conn, temp);
				}
				else if (std::strcmp(params[1], "+o") == 0) {
					std::snprintf(temp, sizeof(temp), "/op %s", params[2]);
					handle_command(conn, temp);
					std::snprintf(temp, sizeof(temp), "%s %s %s", params[0], params[1], params[2]);
					channel_message_send(channel, message_type_mode, conn, temp);
				}
				else if (std::strcmp(params[1], "-o") == 0) {
					std::snprintf(temp, sizeof(temp), "/deop %s", params[2]);
					handle_command(conn, temp);
					std::snprintf(temp, sizeof(temp), "%s %s %s", params[0], params[1], params[2]);
					channel_message_send(channel, message_type_mode, conn, temp);
				}
				else if (std::strcmp(params[1], "+v") == 0) {
					std::snprintf(temp, sizeof(temp), "/voice %s", params[2]);
					handle_command(conn, temp);
					std::snprintf(temp, sizeof(temp), "%s %s %s", params[0], params[1], params[2]);
					channel_message_send(channel, message_type_mode, conn, temp);
				}
				else if (std::strcmp(params[1], "-v") == 0) {
					std::snprintf(temp, sizeof(temp), "/devoice %s", params[2]);
					handle_command(conn, temp);

					std::snprintf(temp, sizeof(temp), "%s %s %s", params[0], params[1], params[2]);
					channel_message_send(channel, message_type_mode, conn, temp);
				}
				else if (std::strcmp(params[1], "+i") == 0) {
					/* FIXME: channel will be only for invited */
				}
				else if (std::strcmp(params[1], "+l") == 0) {
					channel_set_max(channel, std::atoi(params[2]));
					std::snprintf(temp, sizeof(temp), "%s %s %s", params[0], params[1], params[2]);
					channel_message_send(channel, message_type_mode, conn, temp);
				}
				else if (std::strcmp(params[1], "-l") == 0) {
					channel_set_max(channel, -1);
					std::snprintf(temp, sizeof(temp), "%s %s", params[0], params[1]);
					channel_message_send(channel, message_type_mode, conn, temp);
				}
				else {
					std::snprintf(temp, sizeof(temp), ":%s is unknown mode char to me for %s", params[1], params[0]);
					irc_send(conn, ERR_UNKNOWNMODE, temp);
				}
			}
			else {
				/* User modes */
				/* FIXME: Support user modes (away, invisible...) */
				irc_send(conn, ERR_UMODEUNKNOWNFLAG, ":Unknown MODE flag");
			}
			return 0;
		}

		extern int _handle_time_command(t_connection * conn, int numparams, char ** params, char * text)
		{
			char temp[MAX_IRC_MESSAGE_LEN] = {}; // PELISH: According to RFC2812
			std::time_t now;
			char const * ircname = server_get_hostname();

			if ((numparams >= 1) && (params[0]))
			{
				if (std::strcmp(params[0], ircname) == 0)
				{
					now = std::time(NULL);
					std::snprintf(temp, sizeof(temp), "%s :%" PRId64, ircname, static_cast<std::int64_t>(now));
					irc_send(conn, RPL_TIME, temp);
				}
				else {
					std::snprintf(temp, sizeof(temp), "%s :No such server", params[0]);
					irc_send(conn, ERR_NOSUCHSERVER, temp);
				}
			}
			else {
				/* RPL_TIME contains time and name of this server */
				now = std::time(NULL);
				std::snprintf(temp, sizeof(temp), "%s :%" PRId64, ircname, static_cast<std::int64_t>(now));
				irc_send(conn, RPL_TIME, temp);
			}
			return 0;
			}
	
			// ---------------------------------------------------------------------------
			// IRC command handlers — moved from handle_irc.cpp (Round 134)
			// TODO(Phase3): handled by v3 IrcFsm — legacy IRC command handlers inlined here
			// ---------------------------------------------------------------------------
	
			static int _handle_user_command(t_connection * conn, int numparams, char ** params, char * text)
			{
				char * user = NULL;
				char * realname = NULL;
	
				if ((numparams >= 3) && (params[0]) && (text)) {
					user = params[0];
					realname = text;
	
					if (conn_get_user(conn)) {
						irc_send(conn, ERR_ALREADYREGISTRED, ":You are already registred");
					}
					else {
						eventlog(eventlog_level_debug, __FUNCTION__, "[{}] got USER: user=\"{}\" realname=\"{}\"", conn_get_socket(conn), user, realname);
						conn_set_user(conn, user);
						conn_set_owner(conn, realname);
						if (conn_get_loggeduser(conn))
							irc_welcome(conn); /* only send the welcome if we have USER and NICK */
					}
				}
				else {
					irc_send(conn, ERR_NEEDMOREPARAMS, "USER :Not enough parameters");
				}
				return 0;
			}
	
			static int _handle_pass_command(t_connection * conn, int numparams, char ** params, char * text)
			{
				if ((!conn_get_ircpass(conn)) && (conn_get_state(conn) == conn_state_bot_username)) {
					t_hash h;
	
					if (numparams >= 1) {
						bnet_hash(&h, std::strlen(params[0]), params[0]);
						conn_set_ircpass(conn, hash_get_str(h));
					}
					else
						irc_send(conn, ERR_NEEDMOREPARAMS, "PASS :Not enough parameters");
				}
				else {
					irc_send(conn, ERR_ALREADYREGISTRED, ":Unauthorized command (already registered)");
				}
				return 0;
			}
	
			static int _handle_privmsg_command(t_connection * conn, int numparams, char ** params, char * text)
			{
				if ((numparams >= 1) && (text))
				{
					int i;
					char ** e;
	
					e = irc_get_listelems(params[0]);
	
					for (i = 0; ((e) && (e[i])); i++) {
						if (strcasecmp(e[i], "NICKSERV") == 0) {
							char * pass;
	
							pass = std::strchr(text, ' ');
							if (pass)
								*pass++ = '\0';
	
							if (strcasecmp(text, "identify") == 0) {
								switch (conn_get_state(conn)) {
								case conn_state_bot_password:
								{
									if (pass) {
										t_hash h;
	
										strtolower(pass);
										bnet_hash(&h, std::strlen(pass), pass);
										irc_authenticate(conn, hash_get_str(h));
									}
									else {
										message_send_text(conn, message_type_notice, NULL, "Syntax: IDENTIFY <password> (max 16 characters)");
									}
									break;
								}
								case conn_state_loggedin:
								{
									message_send_text(conn, message_type_notice, NULL, "You don't need to IDENTIFY");
									break;
								}
								default:;
									eventlog(eventlog_level_trace, __FUNCTION__, "got /msg in unexpected connection state ({})", conn_state_get_str(conn_get_state(conn)));
								}
							}
							else if (strcasecmp(text, "register") == 0) {
								t_hash       passhash;
								t_account  * temp;
								char       * username = (char *)conn_get_loggeduser(conn);
	
								if (account_check_name(username)<0) {
									message_send_text(conn, message_type_error, conn, "Account name contains invalid symbol!");
									break;
								}
	
								if (!prefs_v3::allow_new_accounts()){
									message_send_text(conn, message_type_error, conn, "Account creation is not allowed");
									break;
								}
	
								if (!pass || pass[0] == '\0' || (std::strlen(pass)>16)) {
									message_send_text(conn, message_type_error, conn, "Syntax: REGISTER <password> (max 16 characters)");
									break;
								}
	
								strtolower(pass);
	
								bnet_hash(&passhash, std::strlen(pass), pass);
	
								message_send_text(conn, message_type_info, conn, std::string("Trying to create account \"" + std::string(username) + "\" with password \"" + std::string(pass) + "\"").c_str());
	
								temp = accountlist_create_account(username, hash_get_str(passhash));
								if (!temp) {
									message_send_text(conn, message_type_error, conn, "Failed to create account!");
									eventlog(eventlog_level_debug, __FUNCTION__, "[{}] account \"{}\" not created (failed)", conn_get_socket(conn), username);
									conn_unget_chatname(conn, username);
									break;
								}
	
								message_send_text(conn, message_type_info, conn, std::string("Account #" + std::to_string(account_get_uid(temp)) + " created."));
								eventlog(eventlog_level_debug, __FUNCTION__, "[{}] account \"{}\" created", conn_get_socket(conn), username);
								conn_unget_chatname(conn, username);
							}
							else {
								message_send_text(conn, message_type_notice, nullptr, "Invalid arguments for NICKSERV");
								message_send_text(conn, message_type_notice, nullptr, std::string(":Unrecognized command \"" + std::string(text) + "\"").c_str());
							}
						}
						else if (conn_get_state(conn) == conn_state_loggedin) {
							if (e[i][0] == '#') {
								t_channel * channel;
	
								if ((channel = channellist_find_channel_by_name(irc_convert_ircname(e[i]), NULL, NULL))) {
									if ((std::strlen(text) >= 9) && (std::strncmp(text, "\001ACTION ", 8) == 0) && (text[std::strlen(text) - 1] == '\001')) {
										text = text + 8;
										text[std::strlen(text) - 1] = '\0';
										channel_message_send(channel, message_type_emote, conn, text);
									}
									else {
										channel_message_log(channel, conn, 1, text);
										channel_message_send(channel, message_type_talk, conn, text);
									}
								}
								else {
									irc_send(conn, ERR_NOSUCHCHANNEL, ":No such channel");
								}
							}
							else {
								t_connection * user;
	
								if ((user = connlist_find_connection_by_accountname(e[i])))
								{
									message_send_text(user, message_type_whisper, conn, text);
								}
								else
								{
									irc_send(conn, ERR_NOSUCHNICK, ":No such user");
								}
							}
						}
					}
					if (e)
						irc_unget_listelems(e);
				}
				else
					irc_send(conn, ERR_NEEDMOREPARAMS, "PRIVMSG :Not enough parameters");
				return 0;
			}
	
			static int _handle_notice_command(t_connection * conn, int numparams, char ** params, char * text)
			{
				if ((numparams >= 1) && (text)) {
					int i;
					char ** e;
	
					e = irc_get_listelems(params[0]);
	
					for (i = 0; ((e) && (e[i])); i++) {
						if (conn_get_state(conn) == conn_state_loggedin) {
							t_connection * user;
	
							if ((user = connlist_find_connection_by_accountname(e[i]))) {
								message_send_text(user, message_type_notice, conn, text);
							}
							else {
								irc_send(conn, ERR_NOSUCHNICK, ":No such user");
							}
						}
					}
					if (e)
						irc_unget_listelems(e);
				}
				else
					irc_send(conn, ERR_NEEDMOREPARAMS, "NOTICE :Not enough parameters");
				return 0;
			}
	
			static int _handle_quit_command(t_connection * conn, int numparams, char ** params, char * text)
			{
				conn_quit_channel(conn, text);
				conn_set_state(conn, conn_state_destroy);
				return 0;
			}
	
			static int _handle_who_command(t_connection * conn, int numparams, char ** params, char * text)
			{
				if (numparams >= 1) {
					int i;
					char ** e;
	
					e = irc_get_listelems(params[0]);
					for (i = 0; ((e) && (e[i])); i++) {
						irc_who(conn, e[i]);
					}
					irc_send(conn, RPL_ENDOFWHO, ":End of WHO list");
					if (e)
						irc_unget_listelems(e);
				}
				else
					irc_send(conn, ERR_NEEDMOREPARAMS, "WHO :Not enough parameters");
				return 0;
			}
	
			static int _handle_list_command(t_connection * conn, int numparams, char ** params, char * text)
			{
				std::string tmp;
				irc_send(conn, RPL_LISTSTART, "Channel :Users Names");
	
				if (numparams == 0)
				{
					class_topic Topic;
					for (t_channel const* channel : channellist())
					{
						char const * tempname = irc_convert_channel(channel, conn);
						std::string topicstr = Topic.get(channel_get_name(channel));
	
						tmp = std::string(tempname) + " " + std::to_string(channel_get_length(channel)) + " :" + topicstr;
	
						if (tmp.length() > MAX_IRC_MESSAGE_LEN)
							eventlog(eventlog_level_warn, __FUNCTION__, "LISTREPLY length exceeded");
	
						irc_send(conn, RPL_LIST, tmp.c_str());
					}
				}
				else if (numparams >= 1)
				{
					int i;
					char ** e;
					class_topic Topic;
	
					e = irc_get_listelems(params[0]);
	
					for (i = 0; ((e) && (e[i])); i++)
					{
						char const * verytemp = irc_convert_ircname(e[i]);
						if (!verytemp)
							continue;
	
						t_channel const * channel = channellist_find_channel_by_name(verytemp, NULL, NULL);
						if (!channel)
							continue;
	
						std::string topicstr = Topic.get(channel_get_name(channel));
						char const * tempname = irc_convert_channel(channel, conn);
	
						tmp = std::string(tempname) + " " + std::to_string(channel_get_length(channel)) + " :" + topicstr;
	
						if (tmp.length() > MAX_IRC_MESSAGE_LEN)
							eventlog(eventlog_level_warn, __FUNCTION__, "LISTREPLY length exceeded");
	
						irc_send(conn, RPL_LIST, tmp.c_str());
					}
	
					if (e)
						irc_unget_listelems(e);
				}
	
				irc_send(conn, RPL_LISTEND, ":End of LIST command");
	
				return 0;
			}
	
			static int _handle_names_command(t_connection * conn, int numparams, char ** params, char * text)
			{
				t_channel * channel;
	
				if (numparams >= 1) {
					char ** e;
					char const * verytemp;
					int i;
	
					e = irc_get_listelems(params[0]);
					for (i = 0; ((e) && (e[i])); i++) {
						verytemp = irc_convert_ircname(e[i]);
	
						if (!verytemp)
							continue;
						channel = channellist_find_channel_by_name(verytemp, NULL, NULL);
						if (!channel)
							continue;
						irc_send_rpl_namreply(conn, channel);
					}
					if (e)
						irc_unget_listelems(e);
				}
				else if (numparams == 0) {
					irc_send_rpl_namreply(conn, NULL);
				}
				return 0;
			}
	
			static int _handle_userhost_command(t_connection * conn, int numparams, char ** params, char * text)
			{
				/* FIXME: Send RPL_USERHOST */
				return 0;
			}
	
			static int _handle_ison_command(t_connection * conn, int numparams, char ** params, char * text)
			{
				char temp[MAX_IRC_MESSAGE_LEN];
				char first = 1;
	
				if (numparams >= 1)
				{
					int i;
	
					temp[0] = '\0';
					for (i = 0; (i < numparams && (params) && (params[i])); i++)
					{
						if (connlist_find_connection_by_accountname(params[i]))
						{
							std::snprintf(temp, sizeof temp, "%s%s", first ? ":" : " ", params[i]);
							first = 0;
						}
					}
					irc_send(conn, RPL_ISON, temp);
				}
				else
					irc_send(conn, ERR_NEEDMOREPARAMS, "ISON :Not enough parameters");
				return 0;
			}
	
			static int _handle_whois_command(t_connection * conn, int numparams, char ** params, char * text)
			{
				std::string tmp;
	
				if (numparams >= 1)
				{
					int i;
					char ** e = irc_get_listelems(params[0]);
					t_connection * c;
					t_channel * chan;
	
					for (i = 0; ((e) && (e[i])); i++)
					{
						if ((c = connlist_find_connection_by_accountname(e[i])))
						{
							if (prefs_v3::hide_addr() && !(account_get_command_groups(conn_get_account(conn)) & command_get_group("/admin-addr")))
								tmp = std::string(e[i]) + " " + std::string(clienttag_uint_to_str(conn_get_clienttag(c))) + " hidden * :PvPGN user";
							else
								tmp = std::string(e[i]) + " " + std::string(clienttag_uint_to_str(conn_get_clienttag(c))) + " " + std::string(addr_num_to_ip_str(conn_get_addr(c))) + " * :PvPGN user";
							irc_send(conn, RPL_WHOISUSER, tmp.c_str());
	
							if ((chan = conn_get_channel(conn)))
							{
								std::string flg;
								auto flags = conn_get_flags(c);
	
								if (flags & MF_BLIZZARD)
									flg = '@';
								else if ((flags & MF_BNET) || (flags & MF_GAVEL))
									flg = '%';
								else if (flags & MF_VOICE)
									flg = '+';
								else
									flg = ' ';
	
								tmp = std::string(e[i]) + " :" + flg + std::string(irc_convert_channel(chan, conn));
								irc_send(conn, RPL_WHOISCHANNELS, tmp.c_str());
							}
	
						}
						else
							irc_send(conn, ERR_NOSUCHNICK, ":No such nick/channel");
	
					}
					irc_send(conn, RPL_ENDOFWHOIS, ":End of /WHOIS list");
					if (e)
						irc_unget_listelems(e);
				}
				else
					irc_send(conn, ERR_NEEDMOREPARAMS, "WHOIS :Not enough parameters");
				return 0;
			}
	
			static int _handle_part_command(t_connection * conn, int numparams, char ** params, char * text)
			{
				conn_part_channel(conn);
				return 0;
			}
	
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
