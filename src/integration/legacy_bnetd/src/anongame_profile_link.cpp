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

// anongame_profile_link.cpp — WAR3 player-profile and clan-profile handlers
// Split from handle_anongame_link.cpp (plan 05 §3 / SOLID-S)
//
// Responsibilities:
//   _client_anongame_profile()      — build WAR3 stats reply (solo/team/ffa/race/AT)
//   _client_anongame_profile_clan() — build clan-profile reply
//
// These are called from handle_anongame_link.cpp via the forward declarations
// in handle_anongame_internal.h.

#include "common/setup_before.h"
#include "handle_anongame.h"

#include <cstring>
#include <cstdio>

#include "common/eventlog.h"
#include "common/bn_type.h"
#include "common/bnettime.h"
#include "common/tag.h"
#include "common/list.h"

#include "clan.h"
#include "account.h"
#include "account_wrap.h"
#include "ladder.h"
#include "team.h"
#include "anongame.h"
#include "server.h"
#include "channel.h"
#include "icons.h"
#include "common/setup_after.h"
#include "prefs_v3_shim.h"

#include "integration/legacy_bnetd/strangler_macros.h"

namespace pvpgn
{

	namespace bnetd
	{

		int _client_anongame_profile_clan(t_connection * c, t_packet const * const packet)
		{
			t_packet * rpacket;
			int clantag;
			int clienttag;
			int count;
			int temp;
			t_clan * clan;
			unsigned char rescount;

			// v3 strangler-fig: reproduce the legacy stub via the
			// typed pipeline so the codec owns the wire format.
			{
				unsigned int sz = packet_get_size(packet);
				void const*  bd = packet_get_data_const(packet, 0, sz);
				if (bd != nullptr) {
					PVPGN_V3_BRIDGE_TRY(clan_profile, c, bd, sz);
				}
			}

			if (packet_get_size(packet) < sizeof(t_client_findanongame_profile_clan))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad ANONGAME_PROFILE_CLAN packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_findanongame_profile_clan), packet_get_size(packet));
				return -1;
			}

			clantag = bn_int_get(packet->u.client_findanongame_profile_clan.clantag);
			clienttag = bn_int_get(packet->u.client_findanongame_profile_clan.clienttag);
			count = bn_int_get(packet->u.server_findanongame_profile_clan.count);
			clan = clanlist_find_clan_by_clantag(clantag);

			if ((rpacket = packet_create(packet_class_bnet)))
			{
				packet_set_size(rpacket, sizeof(t_server_findanongame_profile_clan));
				packet_set_type(rpacket, SERVER_FINDANONGAME_PROFILE_CLAN);
				bn_byte_set(&rpacket->u.server_findanongame_profile_clan.option, CLIENT_FINDANONGAME_PROFILE_CLAN);
				bn_int_set(&rpacket->u.server_findanongame_profile_clan.count, count);

				rescount = 0;

				temp = 0;
				packet_append_data(rpacket, &temp, 1);
				/*
				if (!(clan))
				{
					temp = 0;
					packet_append_data(rpacket, &temp, 1);
				}
				else
				{
					temp = 0;
					packet_append_data(rpacket, &temp, 1);

					/* UNDONE: need to add clan stuff here:
					 format:
					 bn_int	ladder_tag (SNLC, 2NLC, 3NLC, 4NLC)
					 bn_int	wins
					 bn_int	losses
					 bn_byte rank
					 bn_byte progess bar
					 bn_int	xp
					 bn_int	rank
					 bn_byte 0x06 <-- random + 5 races
					 6 times:
					 bn_int  wins
					 bn_int	losses
					 /
				}
				*/

				bn_byte_set(&rpacket->u.server_findanongame_profile_clan.rescount, rescount);


				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			return 0;
		}

		int _client_anongame_profile(t_connection * c, t_packet const * const packet)
		{
			t_packet * rpacket;
			char const * username;
			int Count, i;
			int temp;
			t_account * account;
			t_connection * dest_c;
			t_clienttag ctag;
			char clienttag_str[5];
			t_list * teamlist;
			unsigned char teamcount;
			unsigned char *atcountp;
			t_elem * curr;
			t_team * team;
			t_bnettime bn_time;
			bn_long ltime;

			// v3 strangler-fig: build the WAR3 stats reply via the
			// pure builder + typed codec; only fall through to the
			// legacy assembly below on failure.
			{
				unsigned int sz = packet_get_size(packet);
				void const*  bd = packet_get_data_const(packet, 0, sz);
				if (bd != nullptr) {
					PVPGN_V3_BRIDGE_TRY(profile, c, bd, sz);
				}
			}


			Count = bn_int_get(packet->u.client_findanongame.count);
			eventlog(eventlog_level_info, __FUNCTION__, "[{}] got a FINDANONGAME PROFILE packet", conn_get_socket(c));

			if (!(username = packet_get_str_const(packet, sizeof(t_client_findanongame_profile), MAX_USERNAME_LEN)))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad FINDANONGAME_PROFILE (missing or too long username)", conn_get_socket(c));
				return -1;
			}

			//If no account is found then break
			if (!(account = accountlist_find_account(username)))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "Could not get account - PROFILE");
				return -1;
			}

			if (!(dest_c = connlist_find_connection_by_accountname(username))) {
				eventlog(eventlog_level_debug, __FUNCTION__, "account is offline -  try ll_clienttag");
				if (!(ctag = account_get_ll_clienttag(account))) return -1;
			}
			else
				ctag = conn_get_clienttag(dest_c);

			eventlog(eventlog_level_info, __FUNCTION__, "Looking up {}'s {} Stats.", username, tag_uint_to_str(clienttag_str, ctag));

			if (account_get_ladder_level(account, ctag, ladder_id_solo) <= 0 &&
				account_get_ladder_level(account, ctag, ladder_id_team) <= 0 &&
				account_get_ladder_level(account, ctag, ladder_id_ffa) <= 0 &&
				account_get_teams(account) == NULL)
			{
				eventlog(eventlog_level_info, __FUNCTION__, "{} does not have WAR3 Stats.", username);
				if (!(rpacket = packet_create(packet_class_bnet)))
					return -1;
				packet_set_size(rpacket, sizeof(t_server_findanongame_profile2));
				packet_set_type(rpacket, SERVER_FINDANONGAME_PROFILE);
				bn_byte_set(&rpacket->u.server_findanongame_profile2.option, CLIENT_FINDANONGAME_PROFILE);
				bn_int_set(&rpacket->u.server_findanongame_profile2.count, Count);
				bn_int_set(&rpacket->u.server_findanongame_profile2.icon, account_icon_to_profile_icon(account_get_user_icon(account, ctag), account, ctag));
				bn_byte_set(&rpacket->u.server_findanongame_profile2.rescount, 0);
				temp = 0;
				packet_append_data(rpacket, &temp, 2);
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}
			else // If they do have a profile then:
			{
				int solowins = account_get_ladder_wins(account, ctag, ladder_id_solo);
				int sololoss = account_get_ladder_losses(account, ctag, ladder_id_solo);
				int soloxp = account_get_ladder_xp(account, ctag, ladder_id_solo);
				int sololevel = account_get_ladder_level(account, ctag, ladder_id_solo);
				int solorank = account_get_ladder_rank(account, ctag, ladder_id_solo);

				int teamwins = account_get_ladder_wins(account, ctag, ladder_id_team);
				int teamloss = account_get_ladder_losses(account, ctag, ladder_id_team);
				int teamxp = account_get_ladder_xp(account, ctag, ladder_id_team);
				int teamlevel = account_get_ladder_level(account, ctag, ladder_id_team);
				int teamrank = account_get_ladder_rank(account, ctag, ladder_id_team);

				int ffawins = account_get_ladder_wins(account, ctag, ladder_id_ffa);
				int ffaloss = account_get_ladder_losses(account, ctag, ladder_id_ffa);
				int ffaxp = account_get_ladder_xp(account, ctag, ladder_id_ffa);
				int ffalevel = account_get_ladder_level(account, ctag, ladder_id_ffa);
				int ffarank = account_get_ladder_rank(account, ctag, ladder_id_ffa);

				int humanwins = account_get_racewins(account, W3_RACE_HUMANS, ctag);
				int humanlosses = account_get_racelosses(account, W3_RACE_HUMANS, ctag);
				int orcwins = account_get_racewins(account, W3_RACE_ORCS, ctag);
				int orclosses = account_get_racelosses(account, W3_RACE_ORCS, ctag);
				int undeadwins = account_get_racewins(account, W3_RACE_UNDEAD, ctag);
				int undeadlosses = account_get_racelosses(account, W3_RACE_UNDEAD, ctag);
				int nightelfwins = account_get_racewins(account, W3_RACE_NIGHTELVES, ctag);
				int nightelflosses = account_get_racelosses(account, W3_RACE_NIGHTELVES, ctag);
				int randomwins = account_get_racewins(account, W3_RACE_RANDOM, ctag);
				int randomlosses = account_get_racelosses(account, W3_RACE_RANDOM, ctag);
				int tourneywins = account_get_racewins(account, W3_RACE_DEMONS, ctag);
				int tourneylosses = account_get_racelosses(account, W3_RACE_DEMONS, ctag);

				unsigned char rescount;

				if (!(rpacket = packet_create(packet_class_bnet)))
					return -1;
				packet_set_size(rpacket, sizeof(t_server_findanongame_profile2));
				packet_set_type(rpacket, SERVER_FINDANONGAME_PROFILE);
				bn_byte_set(&rpacket->u.server_findanongame_profile2.option, CLIENT_FINDANONGAME_PROFILE);
				bn_int_set(&rpacket->u.server_findanongame_profile2.count, Count); //job count
				bn_int_set(&rpacket->u.server_findanongame_profile2.icon, account_icon_to_profile_icon(account_get_user_icon(account, ctag), account, ctag));

				rescount = 0;
				if (sololevel > 0) {
					bn_int_set((bn_int*)&temp, 0x534F4C4F); // SOLO backwards
					packet_append_data(rpacket, &temp, 4);
					temp = 0;
					bn_int_set((bn_int*)&temp, solowins);
					packet_append_data(rpacket, &temp, 2); //SOLO WINS
					bn_int_set((bn_int*)&temp, sololoss);
					packet_append_data(rpacket, &temp, 2); // SOLO LOSSES
					bn_int_set((bn_int*)&temp, sololevel);
					packet_append_data(rpacket, &temp, 1); // SOLO LEVEL
					bn_int_set((bn_int*)&temp, account_get_profile_calcs(account, soloxp, sololevel));
					packet_append_data(rpacket, &temp, 1); // SOLO PROFILE CALC
					bn_int_set((bn_int *)&temp, soloxp);
					packet_append_data(rpacket, &temp, 2); // SOLO XP
					bn_int_set((bn_int *)&temp, solorank);
					packet_append_data(rpacket, &temp, 4); // SOLO LADDER RANK
					rescount++;
				}

				if (teamlevel > 0) {
					//below is for team records. Add this after 2v2,3v3,4v4 are done
					bn_int_set((bn_int*)&temp, 0x5445414D);
					packet_append_data(rpacket, &temp, 4);
					bn_int_set((bn_int*)&temp, teamwins);
					packet_append_data(rpacket, &temp, 2);
					bn_int_set((bn_int*)&temp, teamloss);
					packet_append_data(rpacket, &temp, 2);
					bn_int_set((bn_int*)&temp, teamlevel);
					packet_append_data(rpacket, &temp, 1);
					bn_int_set((bn_int*)&temp, account_get_profile_calcs(account, teamxp, teamlevel));

					packet_append_data(rpacket, &temp, 1);
					bn_int_set((bn_int*)&temp, teamxp);
					packet_append_data(rpacket, &temp, 2);
					bn_int_set((bn_int*)&temp, teamrank);
					packet_append_data(rpacket, &temp, 4);
					//done of team game stats
					rescount++;
				}

				if (ffalevel > 0) {
					bn_int_set((bn_int*)&temp, 0x46464120);
					packet_append_data(rpacket, &temp, 4);
					bn_int_set((bn_int*)&temp, ffawins);
					packet_append_data(rpacket, &temp, 2);
					bn_int_set((bn_int*)&temp, ffaloss);
					packet_append_data(rpacket, &temp, 2);
					bn_int_set((bn_int*)&temp, ffalevel);
					packet_append_data(rpacket, &temp, 1);
					bn_int_set((bn_int*)&temp, account_get_profile_calcs(account, ffaxp, ffalevel));
					packet_append_data(rpacket, &temp, 1);
					bn_int_set((bn_int*)&temp, ffaxp);
					packet_append_data(rpacket, &temp, 2);
					bn_int_set((bn_int*)&temp, ffarank);
					packet_append_data(rpacket, &temp, 4);
					//End of FFA Stats
					rescount++;
				}
				/* set result count */
				bn_byte_set(&rpacket->u.server_findanongame_profile2.rescount, rescount);

				bn_int_set((bn_int*)&temp, 0x06); //start of race stats
				packet_append_data(rpacket, &temp, 1);
				bn_int_set((bn_int*)&temp, randomwins);
				packet_append_data(rpacket, &temp, 2); //random wins
				bn_int_set((bn_int*)&temp, randomlosses);
				packet_append_data(rpacket, &temp, 2); //random losses
				bn_int_set((bn_int*)&temp, humanwins);
				packet_append_data(rpacket, &temp, 2); //human wins
				bn_int_set((bn_int*)&temp, humanlosses);
				packet_append_data(rpacket, &temp, 2); //human losses
				bn_int_set((bn_int*)&temp, orcwins);
				packet_append_data(rpacket, &temp, 2); //orc wins
				bn_int_set((bn_int*)&temp, orclosses);
				packet_append_data(rpacket, &temp, 2); //orc losses
				bn_int_set((bn_int*)&temp, undeadwins);
				packet_append_data(rpacket, &temp, 2); //undead wins
				bn_int_set((bn_int*)&temp, undeadlosses);
				packet_append_data(rpacket, &temp, 2); //undead losses
				bn_int_set((bn_int*)&temp, nightelfwins);
				packet_append_data(rpacket, &temp, 2); //elf wins
				bn_int_set((bn_int*)&temp, nightelflosses);
				packet_append_data(rpacket, &temp, 2); //elf losses
				bn_int_set((bn_int*)&temp, tourneywins);
				packet_append_data(rpacket, &temp, 2); //tourney wins
				bn_int_set((bn_int*)&temp, tourneylosses);
				packet_append_data(rpacket, &temp, 2); //tourney losses
				//end of normal stats - Start of AT stats

				/* 1 byte team count place holder, set later */
				packet_append_data(rpacket, &temp, 1);

				/* we need to store the AT team count but we dont know yet the no
				 * of stored teams so we cache the pointer for later use
				 */
				atcountp = (unsigned char *)packet_get_raw_data(rpacket, packet_get_size(rpacket) - 1);

				teamlist = account_get_teams(account);
				teamcount = 0;
				if (teamlist)
				{
					int teamtype[] = { 0, 0x32565332, 0x33565333, 0x34565334, 0x35565335, 0x36565336 };

					LIST_TRAVERSE(teamlist, curr)
					{
						if (!(team = (t_team*)elem_get_data(curr)))
						{
							eventlog(eventlog_level_error, __FUNCTION__, "found NULL entry in list");
							continue;
						}

						if (team_get_clienttag(team) != ctag)
							continue;

						bn_int_set((bn_int*)&temp, teamtype[team_get_size(team) - 1]);
						packet_append_data(rpacket, &temp, 4);

						bn_int_set((bn_int*)&temp, team_get_wins(team)); //at team wins
						packet_append_data(rpacket, &temp, 2);
						bn_int_set((bn_int*)&temp, team_get_losses(team)); //at team losses
						packet_append_data(rpacket, &temp, 2);
						bn_int_set((bn_int*)&temp, team_get_level(team));
						packet_append_data(rpacket, &temp, 1);
						bn_int_set((bn_int*)&temp, account_get_profile_calcs(account, team_get_xp(team), team_get_level(team))); // xp bar calc
						packet_append_data(rpacket, &temp, 1);
						bn_int_set((bn_int*)&temp, team_get_xp(team));
						packet_append_data(rpacket, &temp, 2);
						bn_int_set((bn_int*)&temp, team_get_rank(team)); //rank on AT ladder
						packet_append_data(rpacket, &temp, 4);

						bn_time = time_to_bnettime(temp, team_get_lastgame(team));
						bnettime_to_bn_long(bn_time, &ltime);
						packet_append_data(rpacket, &ltime, 8);

						bn_int_set((bn_int*)&temp, team_get_size(team) - 1);
						packet_append_data(rpacket, &temp, 1);

						for (i = 0; i < team_get_size(team); i++)
						{
							if ((team_get_memberuid(team, i) != account_get_uid(account)))
								packet_append_string(rpacket, account_get_name(team_get_member(team, i)));
							//now attach the names to the packet - not including yourself
							// [quetzal] 20020826

						}
						teamcount++;

						if ((teamcount >= 16)) break;
					}
				}

				*atcountp = (unsigned char)teamcount;

				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);

				eventlog(eventlog_level_info, __FUNCTION__, "Sent {}'s WAR3 Stats (including {} teams) to requestor.", username, teamcount);
			}
			return 0;
		}

	} // namespace bnetd

} // namespace pvpgn
