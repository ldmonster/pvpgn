// =====================================================================
// Auto-split from handle_bnet_link.cpp by scripts/dev/split_handle_bnet.py
// See plans/15-large-file-decomposition-detail.md for rationale.
// =====================================================================
#include "handle_bnet_internal.h"

namespace pvpgn { namespace bnetd {

		int _client_ladderreq(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_ladder_dispatch_try(c, "ladderreq");
			t_packet *rpacket;


			if (packet_get_size(packet) < sizeof(t_client_ladderreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad LADDERREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_ladderreq), packet_get_size(packet));
				return -1;
			}

			{
				t_ladder_entry entry;
				unsigned int i;
				unsigned int type;
				unsigned int start;
				unsigned int count;
				unsigned int idnum;
				t_account *account;
				t_clienttag clienttag;
				char const *timestr;
				t_bnettime bt;
				t_ladder_id id;
				t_ladder_sort sort;
				bool error = false;

				clienttag = conn_get_clienttag(c);

				type = bn_int_get(packet->u.client_ladderreq.type);
				start = bn_int_get(packet->u.client_ladderreq.startplace);
				count = bn_int_get(packet->u.client_ladderreq.count);
				idnum = bn_int_get(packet->u.client_ladderreq.id);

				/* eventlog(eventlog_level_debug,__FUNCTION__,"got LADDERREQ type=%u start=%u count=%u id=%u",type,start,count,id); */

				switch (idnum) {
				case CLIENT_LADDERREQ_ID_STANDARD:
					id = ladder_id_normal;
					break;
				case CLIENT_LADDERREQ_ID_IRONMAN:
					id = ladder_id_ironman;
					break;
				default:
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got unknown ladder ladderreq.id=0x{:08x}", conn_get_socket(c), idnum);
					id = ladder_id_normal;
				}

				if (!(rpacket = packet_create(packet_class_bnet)))
					return -1;
				packet_set_size(rpacket, sizeof(t_server_ladderreply));
				packet_set_type(rpacket, SERVER_LADDERREPLY);

				bn_int_set(&rpacket->u.server_ladderreply.clienttag, clienttag);
				bn_int_set(&rpacket->u.server_ladderreply.id, idnum);
				bn_int_set(&rpacket->u.server_ladderreply.type, type);
				bn_int_set(&rpacket->u.server_ladderreply.startplace, start);
				bn_int_set(&rpacket->u.server_ladderreply.count, count);

				switch (type){
				case CLIENT_LADDERREQ_TYPE_HIGHESTRATED:
					sort = ladder_sort_highestrated;
					break;
				case CLIENT_LADDERREQ_TYPE_MOSTWINS:
					sort = ladder_sort_mostwins;
					break;
				case CLIENT_LADDERREQ_TYPE_MOSTGAMES:
					sort = ladder_sort_mostgames;
					break;
				default:
					sort = ladder_sort_default;
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got unknown value for ladderreq.type={}", conn_get_socket(c), type);
					error = true;
				}

				LadderList* ladderList_active = NULL;
				LadderList* ladderList_current = NULL;

				if (!error)
				{
					ladderList_active = ladders.getLadderList(LadderKey(id, clienttag, sort, ladder_time_active));
					ladderList_current = ladders.getLadderList(LadderKey(id, clienttag, sort, ladder_time_current));
				}
				if (!ladderList_active || ladderList_current)
					error = true;

				for (i = start; i < start + count; i++) {


					const LadderReferencedObject* referencedObject = NULL;
					account = NULL;
					if (!error){

						if (!(referencedObject = ladderList_active->getReferencedObject(i + 1)))
							referencedObject = ladderList_current->getReferencedObject(i + 1);
					}

					if ((referencedObject) && (account = referencedObject->getAccount()))
					{
						bn_int_set(&entry.active.wins, account_get_ladder_active_wins(account, clienttag, id));
						bn_int_set(&entry.active.loss, account_get_ladder_active_losses(account, clienttag, id));
						bn_int_set(&entry.active.disconnect, account_get_ladder_active_disconnects(account, clienttag, id));
						bn_int_set(&entry.active.rating, account_get_ladder_active_rating(account, clienttag, id));
						bn_int_set(&entry.active.rank, account_get_ladder_active_rank(account, clienttag, id) - 1);
						if (!(timestr = account_get_ladder_active_last_time(account, clienttag, id)))
							timestr = BNETD_LADDER_DEFAULT_TIME;
						bnettime_set_str(&bt, timestr);
						bnettime_to_bn_long(bt, &entry.lastgame_active);

						bn_int_set(&entry.current.wins, account_get_ladder_wins(account, clienttag, id));
						bn_int_set(&entry.current.loss, account_get_ladder_losses(account, clienttag, id));
						bn_int_set(&entry.current.disconnect, account_get_ladder_disconnects(account, clienttag, id));
						bn_int_set(&entry.current.rating, account_get_ladder_rating(account, clienttag, id));
						bn_int_set(&entry.current.rank, account_get_ladder_rank(account, clienttag, id) - 1);
						if (!(timestr = account_get_ladder_last_time(account, clienttag, id)))
							timestr = BNETD_LADDER_DEFAULT_TIME;
						bnettime_set_str(&bt, timestr);
						bnettime_to_bn_long(bt, &entry.lastgame_current);
					}
					else {
						bn_int_set(&entry.active.wins, 0);
						bn_int_set(&entry.active.loss, 0);
						bn_int_set(&entry.active.disconnect, 0);
						bn_int_set(&entry.active.rating, 0);
						bn_int_set(&entry.active.rank, 0);
						bn_long_set_a_b(&entry.lastgame_active, 0, 0);

						bn_int_set(&entry.current.wins, 0);
						bn_int_set(&entry.current.loss, 0);
						bn_int_set(&entry.current.disconnect, 0);
						bn_int_set(&entry.current.rating, 0);
						bn_int_set(&entry.current.rank, 0);
						bn_long_set_a_b(&entry.lastgame_current, 0, 0);
					}

					bn_int_set(&entry.ttest[0], i);	// rank
					bn_int_set(&entry.ttest[1], 0);	//
					bn_int_set(&entry.ttest[2], 0);	//
					if (account)
						bn_int_set(&entry.ttest[3], account_get_ladder_high_rating(account, clienttag, id));
					else
						bn_int_set(&entry.ttest[3], 0);
					bn_int_set(&entry.ttest[4], 0);	//
					bn_int_set(&entry.ttest[5], 0);	//

					packet_append_data(rpacket, &entry, sizeof(entry));

					if (account)
						packet_append_string(rpacket, account_get_name(account));
					else
						packet_append_string(rpacket, " ");	/* use a space so the client won't show the user's own account when double-clicked on */
				}

				{
					const unsigned int v3_clienttag =
					    bn_int_get(rpacket->u.server_ladderreply.clienttag);
					const unsigned int v3_id =
					    bn_int_get(rpacket->u.server_ladderreply.id);
					const unsigned int v3_type =
					    bn_int_get(rpacket->u.server_ladderreply.type);
					const unsigned int v3_start =
					    bn_int_get(rpacket->u.server_ladderreply.startplace);
					const unsigned int v3_count =
					    bn_int_get(rpacket->u.server_ladderreply.count);
					if (pvpgn_v3_send_ladderreply(c,
					        v3_clienttag, v3_id, v3_type, v3_start, v3_count,
					        nullptr) == 1) {
						packet_del_ref(rpacket);
						return 0;
					}
				}
				conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
				}
	
				return 0;
			}

		int _client_laddersearchreq(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_ladder_dispatch_try(c, "laddersearchreq");
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_laddersearchreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad LADDERSEARCHREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_laddersearchreq), packet_get_size(packet));
				return -1;
			}

			{
				char const *playername;
				t_account *account;
				unsigned int idnum;
				unsigned int type;
				unsigned int rank;	/* starts at zero */
				t_ladder_id id;
				t_ladder_sort sort;
				t_clienttag ctag = conn_get_clienttag(c);

				idnum = bn_int_get(packet->u.client_laddersearchreq.id);

				switch (idnum) {
				case CLIENT_LADDERREQ_ID_STANDARD:
					id = ladder_id_normal;
					break;
				case CLIENT_LADDERREQ_ID_IRONMAN:
					id = ladder_id_ironman;
					break;
				default:
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got unknown ladder laddersearchreq.id=0x{:08x}", conn_get_socket(c), idnum);
					id = ladder_id_normal;
				}

				type = bn_int_get(packet->u.client_laddersearchreq.type);
				switch (type)  {
				case CLIENT_LADDERSEARCHREQ_TYPE_HIGHESTRATED:
					sort = ladder_sort_highestrated;
					break;
				case CLIENT_LADDERSEARCHREQ_TYPE_MOSTWINS:
					sort = ladder_sort_mostwins;
					break;
				case CLIENT_LADDERSEARCHREQ_TYPE_MOSTGAMES:
					sort = ladder_sort_mostgames;
					break;
				default:
					sort = ladder_sort_default;
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got unknown ladder search type {}", conn_get_socket(c), type);
				}

				if (!(playername = packet_get_str_const(packet, sizeof(t_client_laddersearchreq), MAX_USERNAME_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad LADDERSEARCHREQ packet (missing or too long playername)", conn_get_socket(c));
					return -1;
				}

				if (!(account = accountlist_find_account(playername)))
					rank = SERVER_LADDERSEARCHREPLY_RANK_NONE;
				else {
					LadderList * ladderList_active = ladders.getLadderList(LadderKey(id, ctag, sort, ladder_time_active));
					LadderList * ladderList_current = ladders.getLadderList(LadderKey(id, ctag, sort, ladder_time_current));
					unsigned int uid = account_get_uid(account);
					switch (type) {
					case CLIENT_LADDERSEARCHREQ_TYPE_HIGHESTRATED:
					case CLIENT_LADDERSEARCHREQ_TYPE_MOSTWINS:
					case CLIENT_LADDERSEARCHREQ_TYPE_MOSTGAMES:
						if (!(rank = ladderList_active->getRank(uid)))
						{
							if (!(rank = ladderList_current->getRank(uid)) || ladderList_active->getReferencedObject(rank))
								rank = 0;
						}
						break;
					default:
						rank = 0;
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] got unknown ladder search type {}", conn_get_socket(c), bn_int_get(packet->u.client_laddersearchreq.type));
					}

					if (rank == 0)
						rank = SERVER_LADDERSEARCHREPLY_RANK_NONE;
					else
						rank--;
				}

				if (!(rpacket = packet_create(packet_class_bnet)))
					return -1;
				packet_set_size(rpacket, sizeof(t_server_laddersearchreply));
				packet_set_type(rpacket, SERVER_LADDERSEARCHREPLY);
				bn_int_set(&rpacket->u.server_laddersearchreply.rank, rank);
				if (pvpgn_v3_send_laddersearchreply(c,
				        bn_int_get(rpacket->u.server_laddersearchreply.rank)) == 1) {
					packet_del_ref(rpacket);
					return 0;
				}
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			return 0;
		}

		int _client_mapauthreq1(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_gameport_dispatch_try(c, "mapauthreq1");
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_mapauthreq1)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad MAPAUTHREQ1 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_mapauthreq1), packet_get_size(packet));
				return -1;
			}

			{
				char const *mapname;
				t_game *game;

				if (!(mapname = packet_get_str_const(packet, sizeof(t_client_mapauthreq1), MAP_NAME_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad MAPAUTHREQ1 packet (missing or too long mapname)", conn_get_socket(c));
					return -1;
				}

				game = conn_get_game(c);

				if (game) {
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] map auth requested for map \"{}\" gametype \"{}\"", conn_get_socket(c), mapname, game_type_get_str(game_get_type(game)));
					game_set_mapname(game, mapname);
				}
				else
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] map auth requested when not in a game", conn_get_socket(c));

				if ((rpacket = packet_create(packet_class_bnet))) {
					unsigned int val;

					if (!game) {
						val = SERVER_MAPAUTHREPLY1_NO;
						eventlog(eventlog_level_debug, __FUNCTION__, "[{}] map authorization denied (not in a game)", conn_get_socket(c));
					}
					else if (strcasecmp(game_get_mapname(game), mapname) != 0) {
						val = SERVER_MAPAUTHREPLY1_NO;
						eventlog(eventlog_level_debug, __FUNCTION__, "[{}] map authorization denied (map name \"{}\" does not match game map name \"{}\")", conn_get_socket(c), mapname, game_get_mapname(game));
					}
					else {
						game_set_status(game, game_status_started);

						if (game_get_type(game) == game_type_ladder) {
							val = SERVER_MAPAUTHREPLY1_LADDER_OK;
							eventlog(eventlog_level_debug, __FUNCTION__, "[{}] giving map ladder authorization (in a ladder game)", conn_get_socket(c));
						}
						else if (ladder_check_map(game_get_mapname(game), game_get_maptype(game), conn_get_clienttag(c))) {
							val = SERVER_MAPAUTHREPLY1_LADDER_OK;
							eventlog(eventlog_level_debug, __FUNCTION__, "[{}] giving map ladder authorization (is a ladder map)", conn_get_socket(c));
						}
						else {
							val = SERVER_MAPAUTHREPLY1_OK;
							eventlog(eventlog_level_debug, __FUNCTION__, "[{}] giving map normal authorization", conn_get_socket(c));
						}
					}

					packet_set_size(rpacket, sizeof(t_server_mapauthreply1));
					packet_set_type(rpacket, SERVER_MAPAUTHREPLY1);
					bn_int_set(&rpacket->u.server_mapauthreply1.response, val);
					if (pvpgn_v3_send_mapauthreply1(c, bn_int_get(rpacket->u.server_mapauthreply1.response)) == 1) {
						packet_del_ref(rpacket);
					} else {
						conn_push_outqueue(c, rpacket);
						packet_del_ref(rpacket);
					}
				}
			}

			return 0;
		}

		int _client_mapauthreq2(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_gameport_dispatch_try(c, "mapauthreq2");
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_mapauthreq2)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad MAPAUTHREQ2 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_mapauthreq2), packet_get_size(packet));
				return -1;
			}

			{
				char const *mapname;
				t_game *game;

				if (!(mapname = packet_get_str_const(packet, sizeof(t_client_mapauthreq2), MAP_NAME_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad MAPAUTHREQ2 packet (missing or too long mapname)", conn_get_socket(c));
					return -1;
				}

				game = conn_get_game(c);

				if (game) {
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] map auth requested for map \"{}\" gametype \"{}\"", conn_get_socket(c), mapname, game_type_get_str(game_get_type(game)));
					game_set_mapname(game, mapname);
				}
				else
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] map auth requested when not in a game", conn_get_socket(c));

				if ((rpacket = packet_create(packet_class_bnet))) {
					unsigned int val;

					if (!game) {
						val = SERVER_MAPAUTHREPLY2_NO;
						eventlog(eventlog_level_debug, __FUNCTION__, "[{}] map authorization denied (not in a game)", conn_get_socket(c));
					}
					else if (strcasecmp(game_get_mapname(game), mapname) != 0) {
						val = SERVER_MAPAUTHREPLY2_NO;
						eventlog(eventlog_level_debug, __FUNCTION__, "[{}] map authorization denied (map name \"{}\" does not match game map name \"{}\")", conn_get_socket(c), mapname, game_get_mapname(game));
					}
					else {
						game_set_status(game, game_status_started);

						if (game_get_type(game) == game_type_ladder) {
							val = SERVER_MAPAUTHREPLY2_LADDER_OK;
							eventlog(eventlog_level_debug, __FUNCTION__, "[{}] giving map ladder authorization (in a ladder game)", conn_get_socket(c));
						}
						else if (ladder_check_map(game_get_mapname(game), game_get_maptype(game), conn_get_clienttag(c))) {
							val = SERVER_MAPAUTHREPLY2_LADDER_OK;
							eventlog(eventlog_level_debug, __FUNCTION__, "[{}] giving map ladder authorization (is a ladder map)", conn_get_socket(c));
						}
						else {
							val = SERVER_MAPAUTHREPLY2_OK;
							eventlog(eventlog_level_debug, __FUNCTION__, "[{}] giving map normal authorization", conn_get_socket(c));
						}
					}

					packet_set_size(rpacket, sizeof(t_server_mapauthreply2));
					packet_set_type(rpacket, SERVER_MAPAUTHREPLY2);
					bn_int_set(&rpacket->u.server_mapauthreply2.response, val);
					if (pvpgn_v3_send_mapauthreply2(c, bn_int_get(rpacket->u.server_mapauthreply2.response)) == 1) {
						packet_del_ref(rpacket);
					} else {
						conn_push_outqueue(c, rpacket);
						packet_del_ref(rpacket);
					}
				}
			}

			return 0;
		}


}} // namespace pvpgn::bnetd
