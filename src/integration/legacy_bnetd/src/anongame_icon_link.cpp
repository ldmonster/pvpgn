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

// anongame_icon_link.cpp — WAR3 portrait-icon get/set handlers + validation
// Split from handle_anongame_link.cpp (plan 05 §3 / SOLID-S)
//
// Responsibilities:
//   check_user_icon()            — validate that the requested icon is earned
//   _client_anongame_get_icon()  — build icon-table reply (SID_FINDANONGAME 0x09)
//   _client_anongame_set_icon()  — apply user icon selection (SID_FINDANONGAME 0x0A)

#include "common/setup_before.h"
#include "handle_anongame.h"

#include <cstring>
#include <cstdio>

#include "common/eventlog.h"
#include "common/bn_type.h"
#include "common/tag.h"

#include "account.h"
#include "account_wrap.h"
#include "anongame_infos.h"
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

		/* Check user choice for illegal icon */
		// check user for illegal icon
		// Modified by aancw 16/12/2014
		int check_user_icon(t_account * account, const char * user_icon)
		{
			unsigned int i, len;
			char temp_str[2];
			char user_race;
			int number;

			len = std::strlen(user_icon);
			if (len != 4)
				eventlog(eventlog_level_error, __FUNCTION__, "got invalid user icon '{}'", user_icon);

			for (i = 0; i < len && i < 2; i++)
				temp_str[i] = user_icon[i];

			number = temp_str[0] - '0';
			user_race = temp_str[1];


			int race[] = { W3_RACE_RANDOM, W3_RACE_HUMANS, W3_RACE_ORCS, W3_RACE_UNDEAD, W3_RACE_NIGHTELVES, W3_RACE_DEMONS };
			char race_char[6] = { 'R', 'H', 'O', 'U', 'N', 'D' };
			int icon_pos[5] = { 2, 3, 4, 5, 6 };
			int icon_req_wins[5] = { 25, 150, 350, 750, 1500 };
			int icon_req_wins_tourney[5] = { 10, 75, 150, 250, 500};

			for (int i = 0; i < (int)sizeof(race_char); i++)
			{
				if (user_race == race_char[i])
				{
					// Client will got DCed because of different req win normal and tournament
					// Check if race is tournament or not
					// Tournament req wins is different than normal icon

					if(race_char[i] == 'D')
					{
						for (int j = 0; j < (int)sizeof(icon_pos); j++)
						{
							if (number == icon_pos[j])
							{
								// compare account race wins and require wins for tournament icon
								if (account_get_racewins( account, race[i], account_get_ll_clienttag(account) ) >= icon_req_wins_tourney[j])
									return 1;

								return 0;
							}
						}

					}else
					{
						// When normal icon
						for (int j = 0; j < (int)sizeof(icon_pos); j++)
						{
							if (number == icon_pos[j])
							{
								// compare account race wins and require wins
								if (account_get_racewins( account, race[i], account_get_ll_clienttag(account) ) >= icon_req_wins[j])
									return 1;

								return 0;
							}
						}
					}
				}
			}
			return 0;
		}

		/* Open portrait in Warcraft 3 user profile */
		int _client_anongame_get_icon(t_connection * c, t_packet const * const packet)
		{
			t_packet * rpacket;

			// v3 strangler-fig: try the typed pipeline first.
			// `pvpgn_v3_get_icon` lazily loads the IconReqTable
			// from `prefs_get_anongame_infos_file()`, snapshots the
			// account's icon state, builds the reply table, encodes
			// it via the typed protocol/bnet codec, and dispatches
			// it via `conn_push_outqueue`. Returns non-zero only
			// when it has fully handled the request.
			{
				unsigned int sz = packet_get_size(packet);
				void const*  bd = packet_get_data_const(packet, 0, sz);
				if (bd != nullptr) {
					PVPGN_V3_BRIDGE_TRY(get_icon, c, bd, sz);
				}
			}

			//BlacKDicK 04/20/2003 Need some huge re-work on this.
			{
				struct
				{
					char	 icon_code[4];
					unsigned int portrait_code;
					char	 race;
					bn_short	 required_wins;
					char	 client_enabled;
				} tempicon;

				//FIXME: Add those to the prefs and also merge them on accoun_wrap;
				// FIXED BY DJP 07/16/2003 FOR 110 CHANGE ( TOURNEY & RACE WINS ) + Table_witdh
				short icon_req_race_wins;
				short icon_req_tourney_wins;
				int race[] = { W3_RACE_RANDOM, W3_RACE_HUMANS, W3_RACE_ORCS, W3_RACE_UNDEAD, W3_RACE_NIGHTELVES, W3_RACE_DEMONS };
				char race_char[6] = { 'R', 'H', 'O', 'U', 'N', 'D' };
				char icon_pos[5] = { '2', '3', '4', '5', '6', };
				char table_width = 6;
				char table_height = 5;
				int i, j;
				char rico;
				unsigned int rlvl, rwins;
				t_clienttag clienttag;
				t_account * acc;

				char user_icon[5];
				char const * uicon;

				clienttag = conn_get_clienttag(c);
				acc = conn_get_account(c);
				/* WAR3 uses a different table size, might change if blizzard add tournament support to RoC */
				if (clienttag == CLIENTTAG_WARCRAFT3_UINT) {
					table_width = 5;
					table_height = 4;
				}

				eventlog(eventlog_level_info, __FUNCTION__, "[{}] got FINDANONGAME Get Icons packet", conn_get_socket(c));

				if ((rpacket = packet_create(packet_class_bnet)) == NULL) {
					eventlog(eventlog_level_error, __FUNCTION__, "could not create new packet");
					return -1;
				}

				packet_set_size(rpacket, sizeof(t_server_findanongame_iconreply));
				packet_set_type(rpacket, SERVER_FINDANONGAME_ICONREPLY);
				bn_int_set(&rpacket->u.server_findanongame_iconreply.count, bn_int_get(packet->u.client_findanongame_inforeq.count));
				bn_byte_set(&rpacket->u.server_findanongame_iconreply.option, CLIENT_FINDANONGAME_GET_ICON);

				if (uicon = account_get_user_icon(acc, clienttag))
				{
					std::memcpy(&rpacket->u.server_findanongame_iconreply.curricon, uicon, 4);
				}
				else
				{
					account_get_raceicon(acc, &rico, &rlvl, &rwins, clienttag);
					std::sprintf(user_icon, "%1d%c3W", rlvl, rico);
					std::memcpy(&rpacket->u.server_findanongame_iconreply.curricon, user_icon, 4);
				}

				// if custom stats is enabled then set a custom client icon by player rating
				// do not override user selected icon if any
				bool assignedCustomIcon = false;
				if (!uicon && prefs_v3::custom_icons() == 1 && customicons_allowed_by_client(clienttag))
				{
					if (t_icon_info * icon = customicons_get_icon_by_account(acc, clienttag))
					{
						assignedCustomIcon = true;
						std::memcpy(&rpacket->u.server_findanongame_iconreply.curricon, icon->icon_code, 4);
					}
				}

				bn_byte_set(&rpacket->u.server_findanongame_iconreply.table_width, table_width);
				bn_byte_set(&rpacket->u.server_findanongame_iconreply.table_size, table_width*table_height);
				for (j = 0; j < table_height; j++){
					icon_req_race_wins = anongame_infos_get_ICON_REQ(j + 1, clienttag);
					for (i = 0; i < table_width; i++){
						tempicon.race = i;
						tempicon.icon_code[0] = icon_pos[j];
						tempicon.icon_code[1] = race_char[i];
						tempicon.icon_code[2] = '3';
						tempicon.icon_code[3] = 'W';
						tempicon.portrait_code = (account_icon_to_profile_icon(tempicon.icon_code, acc, clienttag));
						if (i <= 4)
						{
							//Building the icon for the races
							bn_short_set(&tempicon.required_wins, icon_req_race_wins);
							if (assignedCustomIcon || account_get_racewins(acc, race[i], clienttag) < icon_req_race_wins)
								tempicon.client_enabled = 0;
							else
								tempicon.client_enabled = 1;
						}
						else
						{
							//Building the icon for the tourney
							icon_req_tourney_wins = anongame_infos_get_ICON_REQ_TOURNEY(j + 1);
							bn_short_set(&tempicon.required_wins, icon_req_tourney_wins);
							if (assignedCustomIcon || account_get_racewins(acc, race[i], clienttag) < icon_req_tourney_wins)
								tempicon.client_enabled = 0;
							else
								tempicon.client_enabled = 1;
						}
						packet_append_data(rpacket, &tempicon, sizeof(tempicon));
					}
				}
				//Go,go,go
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}
			return 0;
		}

		/* Choose icon by user from profile > portrait */
		int _client_anongame_set_icon(t_connection * c, t_packet const * const packet)
		{
			// v3 strangler-fig: try the typed pipeline first.
			// `pvpgn_v3_set_icon` validates the requested icon
			// against per-account race-win counts (mirrors the
			// legacy `check_user_icon` "ICON SWITCH HACK PROTECTION"),
			// applies via the legacy `account_set_user_icon` /
			// `conn_update_w3_playerinfo` / `channel_rejoin` chain,
			// and returns non-zero only when it has fully handled
			// the request.
			{
				unsigned int sz = packet_get_size(packet);
				void const*  bd = packet_get_data_const(packet, 0, sz);
				if (bd != nullptr) {
					PVPGN_V3_BRIDGE_TRY(set_icon, c, bd, sz);
				}
			}

			//BlacKDicK 04/20/2003
			// Modified by aancw 16/12/2014
			unsigned int desired_icon;
			char user_icon[5];

			t_account * account = conn_get_account(c);
			t_clienttag clienttag = conn_get_clienttag(c);

			// do nothing when custom icon enabled and exists
			if (prefs_v3::custom_icons() == 1 && customicons_allowed_by_client(clienttag) && customicons_get_icon_by_account(account, clienttag))
				return 0;

			/*FIXME: In this case we do not get a 'count' but insted of it we get the icon
			that the client wants to set.'W3H2' for an example. For now it is ok, since they share
			the same position	on the packet*/
			desired_icon = bn_int_get(packet->u.client_findanongame.count);
			//user_icon[4]=0;
			if (desired_icon == 0){
				std::strcpy(user_icon, "1O3W"); // 103W is equal to Default Icon
				eventlog(eventlog_level_info, __FUNCTION__, "[{}] Set icon packet to DEFAULT ICON [{}]", conn_get_socket(c), user_icon);
			}
			else{
				std::memcpy(user_icon, &desired_icon, 4);
				eventlog(eventlog_level_info, __FUNCTION__, "[{}] Set icon packet to ICON [{}]", conn_get_socket(c), user_icon);
			}

			// ICON SWITCH HACK PROTECTION
			if (check_user_icon(account, user_icon) == 0)
			{
				std::strcpy(user_icon, "1O3W"); // set icon to default
				eventlog(eventlog_level_info, __FUNCTION__, "[{}] \"{}\" ICON SWITCH hack attempt, icon set to default ", conn_get_username(c), user_icon);
				//conn_set_state(c,conn_state_destroy); // dont kill user session
			}

			account_set_user_icon(account, clienttag, user_icon);
			//FIXME: Still need a way to 'refresh the user/channel'
			//_handle_rejoin_command(conn_get_account(c),"");
			/* ??? channel_update_userflags() */
			conn_update_w3_playerinfo(c);

			channel_rejoin(c);
			return 0;
		}

	} // namespace bnetd

} // namespace pvpgn
