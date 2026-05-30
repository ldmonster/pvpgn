/*
 * Copyright (C) 2004 CreepLord (creeplord@pvpgn.org)
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

// anongame_tournament_link.cpp — tournament status handler + time conversion
// Split from handle_anongame_link.cpp (plan 05 §3 / SOLID-S)
//
// Responsibilities:
//   _tournament_time_convert()    — convert Unix timestamp to Battle.net wire format
//   _client_anongame_tournament() — build tournament-status reply (SID_FINDANONGAME 0x07)

#include "common/setup_before.h"
#include "handle_anongame.h"

#include "common/eventlog.h"
#include "common/bn_type.h"

#include "account.h"
#include "server.h"
#include "tournament.h"
#include "common/setup_after.h"

#include "integration/legacy_bnetd/strangler_macros.h"

namespace pvpgn
{

	namespace bnetd
	{

		static unsigned int _tournament_time_convert(unsigned int time)
		{
			/* it works, don't ask me how */ /* some time drift reportd by testers */
			unsigned int tmp1, tmp2, tmp3;

			tmp1 = time - 1059179400;	/* 0x3F21CB88  */
			tmp2 = static_cast<unsigned int>(tmp1*0.59604645);
			tmp3 = tmp2 + 3276999960U;
			/*eventlog(eventlog_level_trace,__FUNCTION__,"time: 0x%08x, tmp1: 0x%08x, tmp2 0x%08x, tmp3 0x%08x",time,tmp1,tmp2,tmp3);*/

			return tmp3;
		}

		/* tournament notice disabled at this time, but responce is sent to cleint */
		int _client_anongame_tournament(t_connection * c, t_packet const * const packet)
		{
			t_packet * rpacket;

			// v3 strangler-fig: try the typed pipeline first.
			// `pvpgn_v3_tournament` snapshots the legacy
			// tournament globals + per-account state into a pure
			// `TournamentInputs`, runs `build_tournament_reply`,
			// encodes via the typed protocol/bnet codec, and
			// dispatches via `conn_push_outqueue`.
			{
				unsigned int sz = packet_get_size(packet);
				void const*  bd = packet_get_data_const(packet, 0, sz);
				if (bd != nullptr) {
					PVPGN_V3_BRIDGE_TRY(tournament, c, bd, sz);
				}
			}

			t_account * account = conn_get_account(c);
			t_clienttag clienttag = conn_get_clienttag(c);

			unsigned int start_prelim = tournament_get_start_preliminary();
			unsigned int end_signup = tournament_get_end_signup();
			unsigned int end_prelim = tournament_get_end_preliminary();
			unsigned int start_r1 = tournament_get_start_round_1();

			if ((rpacket = packet_create(packet_class_bnet)) == NULL) {
				eventlog(eventlog_level_error, __FUNCTION__, "could not create new packet");
				return -1;
			}

			packet_set_size(rpacket, sizeof(t_server_anongame_tournament_reply));
			packet_set_type(rpacket, SERVER_FINDANONGAME_TOURNAMENT_REPLY);
			bn_byte_set(&rpacket->u.server_anongame_tournament_reply.option, 7);
			bn_int_set(&rpacket->u.server_anongame_tournament_reply.count,
				bn_int_get(packet->u.client_anongame_tournament_request.count));

			if (!start_prelim || (end_signup <= now && tournament_user_signed_up(account) < 0) ||
				tournament_check_client(clienttag) < 0) { /* No Tournament Notice */
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.type, 0);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.unknown, 0);
				bn_short_set(&rpacket->u.server_anongame_tournament_reply.unknown4, 0);
				bn_int_set(&rpacket->u.server_anongame_tournament_reply.timestamp, 0);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.unknown5, 0);
				bn_short_set(&rpacket->u.server_anongame_tournament_reply.countdown, 0);
				bn_short_set(&rpacket->u.server_anongame_tournament_reply.unknown2, 0);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.wins, 0);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.losses, 0);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.ties, 0);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.unknown3, 0);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.selection, 0);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.descnum, 0);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.nulltag, 0);
			}
			else if (start_prelim >= now) { /* Tournament Notice */
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.type, 1);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.unknown, 0);
				bn_short_set(&rpacket->u.server_anongame_tournament_reply.unknown4, 0x0000); /* random */
				bn_int_set(&rpacket->u.server_anongame_tournament_reply.timestamp, _tournament_time_convert(start_prelim));
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.unknown5, 0x01);
				bn_short_set(&rpacket->u.server_anongame_tournament_reply.countdown, start_prelim - now);
				bn_short_set(&rpacket->u.server_anongame_tournament_reply.unknown2, 0);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.wins, 0);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.losses, 0);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.ties, 0);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.unknown3, 0x00);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.selection, 2);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.descnum, 0);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.nulltag, 0);
			}
			else if (end_signup >= now) { /* Tournament Signup Notice - Play Game Active */
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.type, 2);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.unknown, 0);
				bn_short_set(&rpacket->u.server_anongame_tournament_reply.unknown4, 0x0828); /* random */
				bn_int_set(&rpacket->u.server_anongame_tournament_reply.timestamp, _tournament_time_convert(end_signup));
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.unknown5, 0x01);
				bn_short_set(&rpacket->u.server_anongame_tournament_reply.countdown, end_signup - now);
				bn_short_set(&rpacket->u.server_anongame_tournament_reply.unknown2, 0);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.wins, tournament_get_stat(account, 1));
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.losses, tournament_get_stat(account, 2));
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.ties, tournament_get_stat(account, 3));
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.unknown3, 0x08);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.selection, 2);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.descnum, 0);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.nulltag, 0);
			}
			else if (end_prelim >= now) { /* Tournament Prelim Period - Play Game Active */
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.type, 3);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.unknown, 0);
				bn_short_set(&rpacket->u.server_anongame_tournament_reply.unknown4, 0x0828); /* random */
				bn_int_set(&rpacket->u.server_anongame_tournament_reply.timestamp, _tournament_time_convert(end_prelim));
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.unknown5, 0x01);
				bn_short_set(&rpacket->u.server_anongame_tournament_reply.countdown, end_prelim - now);
				bn_short_set(&rpacket->u.server_anongame_tournament_reply.unknown2, 0);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.wins, tournament_get_stat(account, 1));
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.losses, tournament_get_stat(account, 2));
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.ties, tournament_get_stat(account, 3));
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.unknown3, 0x08);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.selection, 2);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.descnum, 0);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.nulltag, 0);
			}
			else if (start_r1 >= now && (tournament_get_game_in_progress())) { /* Prelim Period Over - Shows user stats (not all prelim games finished) */
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.type, 4);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.unknown, 0);
				bn_short_set(&rpacket->u.server_anongame_tournament_reply.unknown4, 0x0000); /* random */
				bn_int_set(&rpacket->u.server_anongame_tournament_reply.timestamp, _tournament_time_convert(start_r1));
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.unknown5, 0x01);
				bn_short_set(&rpacket->u.server_anongame_tournament_reply.countdown, start_r1 - now);
				bn_short_set(&rpacket->u.server_anongame_tournament_reply.unknown2, 0); /* 00 00 */
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.wins, tournament_get_stat(account, 1));
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.losses, tournament_get_stat(account, 2));
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.ties, tournament_get_stat(account, 3));
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.unknown3, 0x08);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.selection, 2);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.descnum, 0);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.nulltag, 0);
			}
			else if (!(tournament_get_in_finals_status(account))) { /* Prelim Period Over - user did not make finals - Shows user stats */
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.type, 5);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.unknown, 0);
				bn_short_set(&rpacket->u.server_anongame_tournament_reply.unknown4, 0);
				bn_int_set(&rpacket->u.server_anongame_tournament_reply.timestamp, 0);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.unknown5, 0);
				bn_short_set(&rpacket->u.server_anongame_tournament_reply.countdown, 0);
				bn_short_set(&rpacket->u.server_anongame_tournament_reply.unknown2, 0);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.wins, tournament_get_stat(account, 1));
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.losses, tournament_get_stat(account, 2));
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.ties, tournament_get_stat(account, 3));
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.unknown3, 0x04);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.selection, 2);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.descnum, 0);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.nulltag, 0);
			}
			/* cycle through [type-6] & [type-7] packets
			 *
			 * use [type-6] to show client "eliminated" or "continue"
			 *     timestamp , countdown & round number (of next round) must be set if clinet continues
			 *
			 * use [type-7] to make cleint wait for 44FF packet option 1 to start game (A guess, not tested)
			 *
			 * not sure if there is overall winner packet sent at end of last final round
			 */
			// UNDONE: next two conditions never executed
			else if ((0)) { /* User in finals - Shows user stats and start of next round*/
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.type, 6);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.unknown, 0);
				bn_short_set(&rpacket->u.server_anongame_tournament_reply.unknown4, 0x0000);
				bn_int_set(&rpacket->u.server_anongame_tournament_reply.timestamp, _tournament_time_convert(start_r1));
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.unknown5, 0x01);
				bn_short_set(&rpacket->u.server_anongame_tournament_reply.countdown, start_r1 - now);
				bn_short_set(&rpacket->u.server_anongame_tournament_reply.unknown2, 0x0000); /* 00 00 */
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.wins, 4); /* round number */
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.losses, 0); /* 0 = continue , 1= eliminated */
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.ties, 0);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.unknown3, 0x04); /* number of rounds in finals */
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.selection, 2);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.descnum, 0);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.nulltag, 0);
			}
			else if ((0)) { /* user waiting for match to be made */
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.type, 7);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.unknown, 0);
				bn_short_set(&rpacket->u.server_anongame_tournament_reply.unknown4, 0);
				bn_int_set(&rpacket->u.server_anongame_tournament_reply.timestamp, 0);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.unknown5, 0);
				bn_short_set(&rpacket->u.server_anongame_tournament_reply.countdown, 0);
				bn_short_set(&rpacket->u.server_anongame_tournament_reply.unknown2, 0);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.wins, 1); /* round number */
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.losses, 0);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.ties, 0);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.unknown3, 0x04); /* number of finals */
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.selection, 2);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.descnum, 0);
				bn_byte_set(&rpacket->u.server_anongame_tournament_reply.nulltag, 0);
			}

			conn_push_outqueue(c, rpacket);
			packet_del_ref(rpacket);
			return 0;
		}

	} // namespace bnetd

} // namespace pvpgn
