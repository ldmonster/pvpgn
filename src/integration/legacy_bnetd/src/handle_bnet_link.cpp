// =====================================================================
// R209: relocated from src/bnetd/handle_bnet.cpp into the strangler
// legacy_bnetd integration library. The `#ifdef PVPGN_V3_BNETD_INTEGRATION`
// guards that were threaded through this translation unit (R166-R200,
// ~150 sites including R189-style mandatory-v3 if/else blocks) have been
// programmatically stripped by scripts/dev/strip_v3_guards.py because
// the macro is always defined when this file is compiled (it is set on
// the `integration_legacy_bnetd_linked` target). The behaviour under
// the v3 build is identical to the pre-R209 build with the macro on.
//
// R-split: This file is now the dispatcher only (~250 LOC).
// Handler implementations live in handle_bnet/ subdirectory.
// See plans/15-large-file-decomposition-detail.md §3.
// =====================================================================

/*
 * Copyright (C) 1998  Mark Baysinger (mbaysing@ucsd.edu)
 * Copyright (C) 1998,1999,2000,2001  Ross Combs (rocombs@cs.nmsu.edu)
 * Copyright (C) 1999,2000  Rob Crittenden (rcrit@greyoak.com)
 * Copyright (C) 2000,2001  Marco Ziech (mmz@gmx.net)
 * Copyright (C) 2003 Dizzy
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

#include "handle_bnet/handle_bnet_internal.h"

namespace pvpgn
{

	namespace bnetd
	{

		/* connection state connected handler table */
		static const t_htable_row bnet_htable_con[] = {
			{ CLIENT_UNKNOWN_1B, _client_unknown_1b },
			{ CLIENT_COMPINFO1, _client_compinfo1 },
			{ CLIENT_COMPINFO2, _client_compinfo2 },
			{ CLIENT_COUNTRYINFO1, _client_countryinfo1 },
			{ CLIENT_AUTH_INFO, _client_auth_info },
			{ CLIENT_UNKNOWN_2B, _client_unknown2b },
			{ CLIENT_PROGIDENT, _client_progident },
			{ CLIENT_CLOSEGAME, NULL },
			{ CLIENT_CREATEACCOUNT_W3, _client_createaccountw3 },
			{ CLIENT_CREATEACCTREQ1, _client_createacctreq1 },
			{ CLIENT_CREATEACCTREQ2, _client_createacctreq2 },
			{ CLIENT_CHANGEPASSREQ, _client_changepassreq },
			{ CLIENT_ECHOREPLY, _client_echoreply },
			{ CLIENT_AUTHREQ1, _client_authreq1 },
			{ CLIENT_AUTHREQ_109, _client_authreq109 },
			{ CLIENT_REGSNOOPREPLY, _client_regsnoopreply },
			{ CLIENT_ICONREQ, _client_iconreq },
			{ CLIENT_CDKEY, _client_cdkey },
			{ CLIENT_CDKEY2, _client_cdkey2 },
			{ CLIENT_CDKEY3, _client_cdkey3 },
			{ CLIENT_UDPOK, _client_udpok },
			{ CLIENT_FILEINFOREQ, _client_fileinforeq },
			{ CLIENT_STATSREQ, _client_statsreq },
			{ CLIENT_PINGREQ, _client_pingreq },
			{ CLIENT_LOGINREQ1, _client_loginreq1 },
			{ CLIENT_LOGINREQ2, _client_loginreq2 },
			{ CLIENT_LOGINREQ_W3, _client_loginreqw3 },
			{ CLIENT_PASSCHANGEREQ, _client_passchangereq },
			{ CLIENT_PASSCHANGEPROOFREQ, _client_passchangeproofreq },
			{ CLIENT_LOGONPROOFREQ, _client_logonproofreq },
			{ CLIENT_CHANGECLIENT, _client_changeclient },
			{ CLIENT_GETPASSWORDREQ, _client_getpasswordreq },
			{ CLIENT_CHANGEEMAILREQ, _client_changeemailreq },
			{ CLIENT_CRASHDUMP, _client_crashdump },
			{ -1, NULL }
		};

		/* connection state loggedin handlers */
		static const t_htable_row bnet_htable_log[] = {
			{ CLIENT_CHANGEGAMEPORT, _client_changegameport },
			{ CLIENT_FRIENDSLISTREQ, _client_friendslistreq },
			{ CLIENT_FRIENDINFOREQ, _client_friendinforeq },
			{ CLIENT_ARRANGEDTEAM_FRIENDSCREEN, _client_atfriendscreen },
			{ CLIENT_ARRANGEDTEAM_INVITE_FRIEND, _client_atinvitefriend },
			{ CLIENT_ARRANGEDTEAM_ACCEPT_INVITE, _client_atacceptinvite },
			{ CLIENT_ARRANGEDTEAM_ACCEPT_DECLINE_INVITE, _client_atacceptdeclineinvite },
			/* anongame packet (44ff) handled in handle_anongame.c */
			{ CLIENT_FINDANONGAME, handle_anongame_packet },
			{ CLIENT_FILEINFOREQ, _client_fileinforeq },
			{ CLIENT_MOTD_W3, _client_motdw3 },
			{ CLIENT_REALMLISTREQ, _client_realmlistreq },
			{ CLIENT_REALMLISTREQ_110, _client_realmlistreq110 },
			{ CLIENT_PROFILEREQ, _client_profilereq },
			{ CLIENT_REALMJOINREQ_109, _client_realmjoinreq109 },
			{ CLIENT_UNKNOWN_37, _client_charlistreq },
			{ CLIENT_UNKNOWN_39, _client_unknown39 },
			{ CLIENT_ECHOREPLY, _client_echoreply },
			{ CLIENT_PINGREQ, _client_pingreq },
			{ CLIENT_ADREQ, _client_adreq },
			{ CLIENT_ADACK, _client_adack },
			{ CLIENT_ADCLICK, _client_adclick },
			{ CLIENT_ADCLICK2, _client_adclick2 },
			{ CLIENT_READMEMORY, _client_readmemory },
			{ CLIENT_STATSREQ, _client_statsreq },
			{ CLIENT_STATSUPDATE, _client_statsupdate },
			{ CLIENT_PLAYERINFOREQ, _client_playerinforeq },
			{ CLIENT_PROGIDENT2, _client_progident2 },
			{ CLIENT_JOINCHANNEL, _client_joinchannel },
			{ CLIENT_MESSAGE, _client_message },
			{ CLIENT_GAMELISTREQ, _client_gamelistreq },
			{ CLIENT_JOIN_GAME, _client_joingame },
			{ CLIENT_STARTGAME1, _client_startgame1 },
			{ CLIENT_STARTGAME3, _client_startgame3 },
			{ CLIENT_STARTGAME4, _client_startgame4 },
			{ CLIENT_CLOSEGAME, _client_closegame },
			{ CLIENT_CLOSEGAME2, _client_closegame },
			{ CLIENT_GAME_REPORT, _client_gamereport },
			{ CLIENT_LEAVECHANNEL, _client_leavechannel },
			{ CLIENT_LADDERREQ, _client_ladderreq },
			{ CLIENT_LADDERSEARCHREQ, _client_laddersearchreq },
			{ CLIENT_MAPAUTHREQ1, _client_mapauthreq1 },
			{ CLIENT_MAPAUTHREQ2, _client_mapauthreq2 },
			{ CLIENT_CLAN_DISBANDREQ, _client_clan_disbandreq },
			{ CLIENT_CLANMEMBERLIST_REQ, _client_clanmemberlistreq },
			{ CLIENT_CLAN_MOTDCHG, _client_clan_motdchg },
			{ CLIENT_CLAN_MOTDREQ, _client_clan_motdreq },
			{ CLIENT_CLAN_CREATEREQ, _client_clan_createreq },
			{ CLIENT_CLAN_CREATEINVITEREQ, _client_clan_createinvitereq },
			{ CLIENT_CLAN_CREATEINVITEREPLY, _client_clan_createinvitereply },
			{ CLIENT_CLANMEMBER_RANKUPDATE_REQ, _client_clanmember_rankupdatereq },
			{ CLIENT_CLANMEMBER_REMOVE_REQ, _client_clanmember_removereq },
			{ CLIENT_CLAN_MEMBERNEWCHIEFREQ, _client_clan_membernewchiefreq },
			{ CLIENT_CLAN_INVITEREQ, _client_clan_invitereq },
			{ CLIENT_CLAN_INVITEREPLY, _client_clan_invitereply },
			{ CLIENT_CRASHDUMP, _client_crashdump },
			{ CLIENT_SETEMAILREPLY, _client_setemailreply },
			{ CLIENT_CLANINFOREQ, _client_claninforeq },
			{ CLIENT_EXTRAWORK, _client_extrawork },
			{ CLIENT_NULL, NULL },
			{ -1, NULL }
		};

		static int handle(const t_htable_row * htable, int type, t_connection * c, t_packet const *const packet)
		{
			t_htable_row const *p;
			int res = 1;

			for (p = htable; p->type != -1; p++)
			if (p->type == type) {
				res = 0;
				if (p->handler != NULL)
					res = p->handler(c, packet);
				if (res != 2)
					break;		/* return 2 means we want to continue parsing */
			}

			return res;
		}

		extern int handle_bnet_packet(t_connection * c, t_packet const *const packet)
		{
			if (!c) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got NULL connection", conn_get_socket(c));
				return -1;
			}
			if (!packet) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got NULL packet", conn_get_socket(c));
				return -1;
			}
			if (packet_get_class(packet) != packet_class_bnet) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad packet (class {})", conn_get_socket(c), (int)packet_get_class(packet));
				return -1;
			}

			switch (conn_get_state(c)) {
			case conn_state_connected:
				switch (handle(bnet_htable_con, packet_get_type(packet), c, packet)) {
				case 1:
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] unknown (unlogged in) bnet packet type 0x{:04x}, len {}", conn_get_socket(c), packet_get_type(packet), packet_get_size(packet));
					break;
				case -1:
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] (unlogged in) got error handling packet type 0x{:04x}, len {}", conn_get_socket(c), packet_get_type(packet), packet_get_size(packet));
					break;
				};
				break;

			case conn_state_loggedin:
				switch (handle(bnet_htable_log, packet_get_type(packet), c, packet)) {
				case 1:
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] unknown (logged in) bnet packet type 0x{:04x}, len {}", conn_get_socket(c), packet_get_type(packet), packet_get_size(packet));
					break;
				case -1:
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] (logged in) got error handling packet type 0x{:04x}, len {}", conn_get_socket(c), packet_get_type(packet), packet_get_size(packet));
					break;
				};
				break;

			case conn_state_untrusted:
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] unknown (untrusted) bnet packet type 0x{:04x}, len {}", conn_get_socket(c), packet_get_type(packet), packet_get_size(packet));
				break;

			default:
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] invalid login state {}", conn_get_socket(c), conn_get_state(c));
			};

			return 0;
		}

	} // namespace bnetd

} // namespace pvpgn
