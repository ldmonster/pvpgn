// =====================================================================
// Auto-split from handle_bnet_link.cpp by scripts/dev/split_handle_bnet.py
// See plans/15-large-file-decomposition-detail.md for rationale.
// =====================================================================
#include "handle_bnet_internal.h"

namespace pvpgn { namespace bnetd {

		int _glist_cb(t_game * game, void *data)
		{
			struct glist_cbdata *cbdata = (struct glist_cbdata*)data;
			char clienttag_str[5];
			t_server_gamelistreply_game glgame;
			unsigned int addr;
			unsigned short port;
			bn_int game_spacer = { 1, 0, 0, 0 };

			cbdata->tcount++;
			eventlog(eventlog_level_debug, __FUNCTION__, "[{}] considering listing game=\"{}\", pass=\"{}\" clienttag=\"{}\" gtype={}", conn_get_socket(cbdata->c), game_get_name(game), game_get_pass(game), tag_uint_to_str(clienttag_str, game_get_clienttag(game)), (int)game_get_type(game));

			if (prefs_v3::hide_pass_games() && game_get_flag(game) == game_flag_private) {
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] not listing because game is passworded or has private flag", conn_get_socket(cbdata->c));
				return 0;
			}
			if (prefs_v3::hide_started_games() && game_get_status(game) != game_status_open) {
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] not listing because game is not open", conn_get_socket(cbdata->c));
				return 0;
			}
			if (game_get_clienttag(game) != conn_get_clienttag(cbdata->c)) {
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] not listing because game is for a different client", conn_get_socket(cbdata->c));
				return 0;
			}
			if (cbdata->gtype != game_type_all && game_get_type(game) != cbdata->gtype) {
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] not listing because game is wrong type", conn_get_socket(cbdata->c));
				return 0;
			}
			if (conn_get_versioncheck(cbdata->c) &&
				conn_get_versioncheck(game_get_owner(game)) &&
				conn_get_versioncheck(cbdata->c)->get_version_tag() != conn_get_versioncheck(game_get_owner(game))->get_version_tag())
			{
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] not listing because game is wrong versiontag", conn_get_socket(cbdata->c));
				return 0;
			}

			bn_short_set(&glgame.gametype, gtype_to_bngtype(game_get_type(game)));
			bn_short_set(&glgame.unknown1, SERVER_GAMELISTREPLY_GAME_UNKNOWN1);
			bn_short_set(&glgame.unknown3, SERVER_GAMELISTREPLY_GAME_UNKNOWN3);
			addr = game_get_addr(game);
			port = game_get_port(game);
			trans_net(conn_get_addr(cbdata->c), &addr, &port);
			bn_short_nset(&glgame.port, port);
			bn_int_nset(&glgame.game_ip, addr);
			bn_int_set(&glgame.unknown4, SERVER_GAMELISTREPLY_GAME_UNKNOWN4);
			bn_int_set(&glgame.unknown5, SERVER_GAMELISTREPLY_GAME_UNKNOWN5);
			switch (game_get_status(game)) {
			case game_status_started:
				bn_int_set(&glgame.status, SERVER_GAMELISTREPLY_GAME_STATUS_STARTED);
				break;
			case game_status_full:
				bn_int_set(&glgame.status, SERVER_GAMELISTREPLY_GAME_STATUS_FULL);
				break;
			case game_status_open:
				bn_int_set(&glgame.status, SERVER_GAMELISTREPLY_GAME_STATUS_OPEN);
				break;
			case game_status_done:
				bn_int_set(&glgame.status, SERVER_GAMELISTREPLY_GAME_STATUS_DONE);
				break;
			default:
				eventlog(eventlog_level_warn, __FUNCTION__, "[{}] game \"{}\" has bad status={}", conn_get_socket(cbdata->c), game_get_name(game), (int)game_get_status(game));
				bn_int_set(&glgame.status, 0);
			}
			bn_int_set(&glgame.unknown6, SERVER_GAMELISTREPLY_GAME_UNKNOWN6);

			if (packet_get_size(cbdata->rpacket) + sizeof(glgame)+std::strlen(game_get_name(game)) + 1 + std::strlen(game_get_pass(game)) + 1 + std::strlen(game_get_info(game)) + 1 > MAX_PACKET_SIZE) {
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] out of room for games", conn_get_socket(cbdata->c));
				return -1;			/* no more room */
			}

			if (cbdata->counter) {
				packet_append_data(cbdata->rpacket, &game_spacer, sizeof(game_spacer));
			}

			packet_append_data(cbdata->rpacket, &glgame, sizeof(glgame));
			packet_append_string(cbdata->rpacket, game_get_name(game));
			packet_append_string(cbdata->rpacket, game_get_pass(game));
			packet_append_string(cbdata->rpacket, game_get_info(game));
			cbdata->counter++;

			return 0;
		}

		int _client_gamelistreq(t_connection * c, t_packet const *const packet)
		{
			t_packet *rpacket;
			char const *gamename;
			char const *gamepass;
			unsigned short bngtype;
			t_game_type gtype;
			t_clienttag clienttag;
			t_game *game;
			t_server_gamelistreply_game glgame;
			unsigned int addr;
			unsigned short port;
			char clienttag_str[5];

			if (packet_get_size(packet) < sizeof(t_client_gamelistreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad GAMELISTREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_gamelistreq), packet_get_size(packet));
				return -1;
			}

			if (!(gamename = packet_get_str_const(packet, sizeof(t_client_gamelistreq), MAX_GAMENAME_LEN))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad GAMELISTREQ (missing or too long gamename)", conn_get_socket(c));
				return -1;
			}

			if (!(gamepass = packet_get_str_const(packet, sizeof(t_client_gamelistreq)+std::strlen(gamename) + 1, MAX_GAMEPASS_LEN))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad GAMELISTREQ (missing or too long password)", conn_get_socket(c));
				return -1;
			}

			bngtype = bn_short_get(packet->u.client_gamelistreq.gametype);
			clienttag = conn_get_clienttag(c);
			gtype = bngreqtype_to_gtype(clienttag, bngtype);
			(void)pvpgn_v3_gamelistreq(c, gamename,
			    static_cast<unsigned int>(bngtype));
			if (!(rpacket = packet_create(packet_class_bnet)))
				return -1;
			packet_set_size(rpacket, sizeof(t_server_gamelistreply));
			packet_set_type(rpacket, SERVER_GAMELISTREPLY);

			bn_int_set(&rpacket->u.server_gamelistreply.sstatus, 0);

			/* specific game requested? */
			if (gamename[0] != '\0') {
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] GAMELISTREPLY looking for specific game tag=\"{}\" bngtype=0x{:08x} gtype={} name=\"{}\" pass=\"{}\"", conn_get_socket(c), tag_uint_to_str(clienttag_str, clienttag), bngtype, (int)gtype, gamename, gamepass);
				if ((game = gamelist_find_game(gamename, clienttag, gtype))) {
					/* game found but first we need to make sure everything is OK */
					bn_int_set(&rpacket->u.server_gamelistreply.gamecount, 0);
					switch (game_get_status(game)) {
					case game_status_started:
						bn_int_set(&rpacket->u.server_gamelistreply.sstatus, SERVER_GAMELISTREPLY_GAME_SSTATUS_STARTED);
						eventlog(eventlog_level_debug, __FUNCTION__, "[{}] GAMELISTREPLY found but started", conn_get_socket(c));
						break;
					case game_status_full:
						bn_int_set(&rpacket->u.server_gamelistreply.sstatus, SERVER_GAMELISTREPLY_GAME_SSTATUS_FULL);
						eventlog(eventlog_level_debug, __FUNCTION__, "[{}] GAMELISTREPLY found but full", conn_get_socket(c));
						break;
					case game_status_done:
						bn_int_set(&rpacket->u.server_gamelistreply.sstatus, SERVER_GAMELISTREPLY_GAME_SSTATUS_NOTFOUND);
						eventlog(eventlog_level_debug, __FUNCTION__, "[{}] GAMELISTREPLY found but done", conn_get_socket(c));
						break;
					case game_status_open:
					case game_status_loaded:
						if (std::strcmp(gamepass, game_get_pass(game))) {	/* passworded game must match password in request */
							bn_int_set(&rpacket->u.server_gamelistreply.sstatus, SERVER_GAMELISTREPLY_GAME_SSTATUS_PASS);
							eventlog(eventlog_level_debug, __FUNCTION__, "[{}] GAMELISTREPLY found but is password protected and wrong password given", conn_get_socket(c));
							break;
						}

						if (game_get_status(game) == game_status_loaded) {
							bn_int_set(&rpacket->u.server_gamelistreply.sstatus, SERVER_GAMELISTREPLY_GAME_SSTATUS_LOADED);
							eventlog(eventlog_level_debug, __FUNCTION__, "[{}] GAMELISTREPLY found loaded game", conn_get_socket(c));
						}

						/* everything seems fine, lets reply with the found game */
						bn_int_set(&glgame.status, SERVER_GAMELISTREPLY_GAME_STATUS_OPEN);
						bn_short_set(&glgame.gametype, gtype_to_bngtype(game_get_type(game)));
						bn_short_set(&glgame.unknown1, SERVER_GAMELISTREPLY_GAME_UNKNOWN1);
						bn_short_set(&glgame.unknown3, SERVER_GAMELISTREPLY_GAME_UNKNOWN3);
						addr = game_get_addr(game);
						port = game_get_port(game);
						trans_net(conn_get_addr(c), &addr, &port);
						bn_short_nset(&glgame.port, port);
						bn_int_nset(&glgame.game_ip, addr);
						bn_int_set(&glgame.unknown4, SERVER_GAMELISTREPLY_GAME_UNKNOWN4);
						bn_int_set(&glgame.unknown5, SERVER_GAMELISTREPLY_GAME_UNKNOWN5);
						bn_int_set(&glgame.unknown6, SERVER_GAMELISTREPLY_GAME_UNKNOWN6);

						packet_append_data(rpacket, &glgame, sizeof(glgame));
						packet_append_string(rpacket, game_get_name(game));
						packet_append_string(rpacket, game_get_pass(game));
						packet_append_string(rpacket, game_get_info(game));
						bn_int_set(&rpacket->u.server_gamelistreply.gamecount, 1);
						eventlog(eventlog_level_debug, __FUNCTION__, "[{}] GAMELISTREPLY specific game found", conn_get_socket(c));
						break;
					default:
						eventlog(eventlog_level_warn, __FUNCTION__, "[{}] game \"{}\" has bad status {}", conn_get_socket(c), game_get_name(game), game_get_status(game));
					}
				}
				else {
					bn_int_set(&rpacket->u.server_gamelistreply.gamecount, 0);
					eventlog(eventlog_level_debug, __FUNCTION__, "[{}] GAMELISTREPLY specific game doesn't seem to exist", conn_get_socket(c));
				}
			}
			else {			/* list all public games of this type */
				struct glist_cbdata cbdata;

				if (gtype == game_type_all)
					eventlog(eventlog_level_debug, __FUNCTION__, "GAMELISTREPLY looking for public games tag=\"{}\" bngtype=0x{:08x} gtype=all", tag_uint_to_str(clienttag_str, clienttag), bngtype);
				else
					eventlog(eventlog_level_debug, __FUNCTION__, "GAMELISTREPLY looking for public games tag=\"{}\" bngtype=0x{:08x} gtype={}", tag_uint_to_str(clienttag_str, clienttag), bngtype, (int)gtype);

				cbdata.counter = 0;
				cbdata.tcount = 0;
				cbdata.c = c;
				cbdata.gtype = gtype;
				cbdata.rpacket = rpacket;
				gamelist_traverse(_glist_cb, &cbdata, gamelist_source_joinbutton);

				bn_int_set(&rpacket->u.server_gamelistreply.gamecount, cbdata.counter);
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] GAMELISTREPLY sent {} of {} games", conn_get_socket(c), cbdata.counter, cbdata.tcount);
			}

			{
				const unsigned int v3_sstatus =
				    bn_int_get(rpacket->u.server_gamelistreply.sstatus);
				if (pvpgn_v3_send_gamelistreply(c,
				        static_cast<std::uint32_t>(v3_sstatus),
				        nullptr, 0u) == 1) {
					packet_del_ref(rpacket);
					return 0;
				}
			}
			conn_push_outqueue(c, rpacket);
			packet_del_ref(rpacket);

			return 0;
		}

		int _client_joingame(t_connection * c, t_packet const *const packet)
		{
			char const *gamename;
			char const *gamepass;
			t_game *game;
			t_game_type gtype;

			if (packet_get_size(packet) < sizeof(t_client_join_game)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad JOIN_GAME packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_join_game), packet_get_size(packet));
				return -1;
			}

			if (!(gamename = packet_get_str_const(packet, sizeof(t_client_join_game), MAX_GAMENAME_LEN))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLIENT_JOIN_GAME (missing or too long gamename)", conn_get_socket(c));
				return -1;
			}

			if (!(gamepass = packet_get_str_const(packet, sizeof(t_client_join_game)+std::strlen(gamename) + 1, MAX_GAMEPASS_LEN))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLIENT_JOIN_GAME packet (missing or too long gamepass)", conn_get_socket(c));
				return -1;
			}

			eventlog(eventlog_level_debug, __FUNCTION__, "[{}] trying to join game \"{}\" pass=\"{}\"", conn_get_socket(c), gamename, gamepass);
			(void)pvpgn_v3_joingame(c, gamename);

			if (conn_get_joingamewhisper_ack(c) == 0) {
				watchlist->dispatch(conn_get_account(c), gamename, conn_get_clienttag(c), Watch::ET_joingame);
				conn_set_joingamewhisper_ack(c, 1);	/* 1 = already whispered. We reset this each time user joins a channel */
				clanmember_on_change_status_by_connection(c);
			}

			if (conn_get_channel(c))
				conn_part_channel(c);

			if (!std::strcmp(gamename, "BNet") && !handle_anongame_join(c)) {
				gtype = game_type_anongame;
				gamename = NULL;
				return 0;		/* tmp: do not record any anongames as yet */
			}
			else {
				if (!(game = gamelist_find_game_available(gamename, conn_get_clienttag(c), game_type_all))) {
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] unable to find game \"{}\" for user to join", conn_get_socket(c), gamename);
					return 0;
				}
				gtype = game_get_type(game);
				gamename = game_get_name(game);
				if ((gtype == game_type_ladder && account_get_auth_joinladdergame(conn_get_account(c)) == 0) ||	/* default to true */
					(gtype != game_type_ladder && account_get_auth_joinnormalgame(conn_get_account(c)) == 0)) {	/* default to true */
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] game join for \"{}\" to \"{}\" refused (no authority)", conn_get_socket(c), conn_get_username(c), gamename);
					/* If the user is not in a game, then map authorization
					   will fail and keep them from playing. */
					return 0;
				}
			}

			if (conn_set_game(c, gamename, gamepass, "", gtype, STARTVER_UNKNOWN) < 0)
				eventlog(eventlog_level_info, __FUNCTION__, "[{}] \"{}\" joined game \"{}\", but could not be recorded on server", conn_get_socket(c), conn_get_username(c), gamename);
			else
				eventlog(eventlog_level_info, __FUNCTION__, "[{}] \"{}\" joined game \"{}\"", conn_get_socket(c), conn_get_username(c), gamename);

#ifdef WITH_LUA
			lua_handle_game(game, c, luaevent_game_userjoin);
#endif

			return 0;
		}

		int _client_startgame1(t_connection * c, t_packet const *const packet)
		{
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_startgame1)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STARTGAME1 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_startgame1), packet_get_size(packet));
				return -1;
			}

			{
				char const *gamename;
				char const *gamepass;
				char const *gameinfo;
				unsigned short bngtype;
				unsigned int status;
				t_game *currgame;

				if (!(gamename = packet_get_str_const(packet, sizeof(t_client_startgame1), MAX_GAMENAME_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STARTGAME1 packet (missing or too long gamename)", conn_get_socket(c));
					return -1;
				}
				if (!(gamepass = packet_get_str_const(packet, sizeof(t_client_startgame1)+std::strlen(gamename) + 1, MAX_GAMEPASS_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STARTGAME1 packet (missing or too long gamepass)", conn_get_socket(c));
					return -1;
				}
				if (!(gameinfo = packet_get_str_const(packet, sizeof(t_client_startgame1)+std::strlen(gamename) + 1 + std::strlen(gamepass) + 1, MAX_GAMEINFO_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STARTGAME1 packet (missing or too long gameinfo)", conn_get_socket(c));
					return -1;
				}
				if (conn_get_joingamewhisper_ack(c) == 0) {
					if (watchlist->dispatch(conn_get_account(c), gamename, conn_get_clienttag(c), Watch::ET_joingame) == 0)
						eventlog(eventlog_level_info, "handle_bnet", "Told Mutual Friends your in game {}", gamename);

					conn_set_joingamewhisper_ack(c, 1);	//1 = already whispered. We reset this each time user joins a channel
				}


				bngtype = bn_short_get(packet->u.client_startgame1.gametype);
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] got startgame1 status for game \"{}\" is 0x{:08x} (gametype = 0x{:04x})", conn_get_socket(c), gamename, bn_int_get(packet->u.client_startgame1.status), bngtype);
				status = bn_int_get(packet->u.client_startgame1.status) & CLIENT_STARTGAME1_STATUSMASK;
				(void)pvpgn_v3_startgame(c, 1u, gamename, gameinfo,
				    static_cast<unsigned int>(bngtype), status, 0u, 0u);

				if ((currgame = conn_get_game(c))) {
					switch (status) {
					case CLIENT_STARTGAME1_STATUS_STARTED:
						game_set_status(currgame, game_status_started);
						break;
					case CLIENT_STARTGAME1_STATUS_FULL:
						game_set_status(currgame, game_status_full);
						break;
					case CLIENT_STARTGAME1_STATUS_OPEN:
						game_set_status(currgame, game_status_open);
						break;
					case CLIENT_STARTGAME1_STATUS_DONE:
						game_set_status(currgame, game_status_done);
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] game \"{}\" is finished", conn_get_socket(c), gamename);
						break;
					}
				}
				else if (status != CLIENT_STARTGAME1_STATUS_DONE) {
					t_game_type gtype;

					gtype = bngtype_to_gtype(conn_get_clienttag(c), bngtype);
					if ((gtype == game_type_ladder && account_get_auth_createladdergame(conn_get_account(c)) == 0) ||	/* default to true */
						(gtype != game_type_ladder && account_get_auth_createnormalgame(conn_get_account(c)) == 0))	/* default to true */
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] game start for \"{}\" refused (no authority)", conn_get_socket(c), conn_get_username(c));
					else
						conn_set_game(c, gamename, gamepass, gameinfo, gtype, STARTVER_GW1);

					if ((rpacket = packet_create(packet_class_bnet))) {
						packet_set_size(rpacket, sizeof(t_server_startgame1_ack));
						packet_set_type(rpacket, SERVER_STARTGAME1_ACK);
	
						if (conn_get_game(c))
							bn_int_set(&rpacket->u.server_startgame1_ack.reply, SERVER_STARTGAME1_ACK_OK);
						else
							bn_int_set(&rpacket->u.server_startgame1_ack.reply, SERVER_STARTGAME1_ACK_NO);
	
						if (pvpgn_v3_send_startgame1ack(c,
						        bn_int_get(rpacket->u.server_startgame1_ack.reply)) == 1) {
							packet_del_ref(rpacket);
						} else {
							conn_push_outqueue(c, rpacket);
							packet_del_ref(rpacket);
						}
					}
				}
				else
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] client tried to set game status DONE to destroyed game", conn_get_socket(c));
			}

			return 0;
		}

		int _client_startgame3(t_connection * c, t_packet const *const packet)
		{
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_startgame3)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STARTGAME3 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_startgame3), packet_get_size(packet));
				return -1;
			}

			{
				char const *gamename;
				char const *gamepass;
				char const *gameinfo;
				unsigned short bngtype;
				unsigned int status;
				t_game *currgame;

				if (!(gamename = packet_get_str_const(packet, sizeof(t_client_startgame3), MAX_GAMENAME_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STARTGAME3 packet (missing or too long gamename)", conn_get_socket(c));
					return -1;
				}
				if (!(gamepass = packet_get_str_const(packet, sizeof(t_client_startgame3)+std::strlen(gamename) + 1, MAX_GAMEPASS_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STARTGAME3 packet (missing or too long gamepass)", conn_get_socket(c));
					return -1;
				}
				if (!(gameinfo = packet_get_str_const(packet, sizeof(t_client_startgame3)+std::strlen(gamename) + 1 + std::strlen(gamepass) + 1, MAX_GAMEINFO_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STARTGAME3 packet (missing or too long gameinfo)", conn_get_socket(c));
					return -1;
				}
				if (conn_get_joingamewhisper_ack(c) == 0) {
					if (watchlist->dispatch(conn_get_account(c), gamename, conn_get_clienttag(c), Watch::ET_joingame) == 0)
						eventlog(eventlog_level_info, "handle_bnet", "Told Mutual Friends your in game {}", gamename);

					conn_set_joingamewhisper_ack(c, 1);	//1 = already whispered. We reset this each time user joins a channel
				}
				bngtype = bn_short_get(packet->u.client_startgame3.gametype);
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] got startgame3 status for game \"{}\" is 0x{:08x} (gametype = 0x{:04x})", conn_get_socket(c), gamename, bn_int_get(packet->u.client_startgame3.status), bngtype);
				status = bn_int_get(packet->u.client_startgame3.status) & CLIENT_STARTGAME3_STATUSMASK;
				(void)pvpgn_v3_startgame(c, 3u, gamename, gameinfo,
				    static_cast<unsigned int>(bngtype), status, 0u, 0u);

				if ((currgame = conn_get_game(c))) {
					switch (status) {
					case CLIENT_STARTGAME3_STATUS_STARTED:
						game_set_status(currgame, game_status_started);
						break;
					case CLIENT_STARTGAME3_STATUS_FULL:
						game_set_status(currgame, game_status_full);
						break;
					case CLIENT_STARTGAME3_STATUS_OPEN1:
					case CLIENT_STARTGAME3_STATUS_OPEN:
						game_set_status(currgame, game_status_open);
						break;
					case CLIENT_STARTGAME3_STATUS_DONE:
						game_set_status(currgame, game_status_done);
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] game \"{}\" is finished", conn_get_socket(c), gamename);
						break;
					}
				}
				else if (status != CLIENT_STARTGAME3_STATUS_DONE) {
					t_game_type gtype;

					gtype = bngtype_to_gtype(conn_get_clienttag(c), bngtype);
					if ((gtype == game_type_ladder && account_get_auth_createladdergame(conn_get_account(c)) == 0) || (gtype != game_type_ladder && account_get_auth_createnormalgame(conn_get_account(c)) == 0))
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] game start for \"{}\" refused (no authority)", conn_get_socket(c), conn_get_username(c));
					else
						conn_set_game(c, gamename, gamepass, gameinfo, gtype, STARTVER_GW3);

					if ((rpacket = packet_create(packet_class_bnet))) {
						packet_set_size(rpacket, sizeof(t_server_startgame3_ack));
						packet_set_type(rpacket, SERVER_STARTGAME3_ACK);
	
						if (conn_get_game(c))
							bn_int_set(&rpacket->u.server_startgame3_ack.reply, SERVER_STARTGAME3_ACK_OK);
						else
							bn_int_set(&rpacket->u.server_startgame3_ack.reply, SERVER_STARTGAME3_ACK_NO);
	
						if (pvpgn_v3_send_startgame3ack(c,
						        bn_int_get(rpacket->u.server_startgame3_ack.reply)) == 1) {
							packet_del_ref(rpacket);
						} else {
							conn_push_outqueue(c, rpacket);
							packet_del_ref(rpacket);
						}
					}
				}
				else
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] client tried to set game status DONE to destroyed game", conn_get_socket(c));
			}

			return 0;
		}

		int _client_startgame4(t_connection * c, t_packet const *const packet)
		{
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_startgame4)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STARTGAME4 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_startgame4), packet_get_size(packet));
				return -1;
			}

			if (conn_get_clienttag(c) == CLIENTTAG_STARCRAFT_UINT || conn_get_clienttag(c) == CLIENTTAG_BROODWARS_UINT)
			{
				// FIXME: (HarpyWar) Protection from hack attempt
				// Large map name size will cause crash Starcraft client for user who select an item in game list ("Join" area)
				// It occurs when the packet size of packet 0x0c in length interval 161-164
				// https://github.com/pvpgn/pvpgn-server/issues/159
				if (packet_get_size(packet) > 160)
				{
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got abnormal STARTGAME4 packet length (got {} bytes, hack attempt?)", conn_get_socket(c), packet_get_size(packet));
					return -1;
				}
			}

			// Quick hack to make W3 part channels when creating a game
			if (conn_get_channel(c))
				conn_part_channel(c);

			{
				char const *gamename;
				char const *gamepass;
				char const *gameinfo;
				unsigned short bngtype;
				unsigned int status;
				unsigned int flag;
				unsigned short option;
				t_game *currgame;

				if (!(gamename = packet_get_str_const(packet, sizeof(t_client_startgame4), MAX_GAMENAME_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STARTGAME4 packet (missing or too long gamename)", conn_get_socket(c));
					return -1;
				}
				if (!(gamepass = packet_get_str_const(packet, sizeof(t_client_startgame4)+std::strlen(gamename) + 1, MAX_GAMEPASS_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STARTGAME4 packet (missing or too long gamepass)", conn_get_socket(c));
					return -1;
				}
				if (!(gameinfo = packet_get_str_const(packet, sizeof(t_client_startgame4)+std::strlen(gamename) + 1 + std::strlen(gamepass) + 1, MAX_GAMEINFO_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STARTGAME4 packet (missing or too long gameinfo)", conn_get_socket(c));
					return -1;
				}
				if (conn_get_joingamewhisper_ack(c) == 0) {
					if (watchlist->dispatch(conn_get_account(c), gamename, conn_get_clienttag(c), Watch::ET_joingame) == 0)
						eventlog(eventlog_level_info, "handle_bnet", "Told Mutual Friends your in game {}", gamename);

					conn_set_joingamewhisper_ack(c, 1);	//1 = already whispered. We reset this each time user joins a channel
				}
				bngtype = bn_short_get(packet->u.client_startgame4.gametype);
				option = bn_short_get(packet->u.client_startgame4.option);
				status = bn_int_get(packet->u.client_startgame4.status);
				flag = bn_short_get(packet->u.client_startgame4.flag);

				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] got startgame4 status for game \"{}\" is 0x{:08x} (gametype=0x{:04x} option=0x{:04x}, flag=0x{:04x})", conn_get_socket(c), gamename, status, bngtype, option, flag);
				(void)pvpgn_v3_startgame(c, 4u, gamename, gameinfo,
				    static_cast<unsigned int>(bngtype), status,
				    static_cast<unsigned int>(flag),
				    static_cast<unsigned int>(option));

				if ((currgame = conn_get_game(c))) {
					if ((status & CLIENT_STARTGAME4_STATUSMASK_OPEN_VALID) == status) {
						if (status & CLIENT_STARTGAME4_STATUS_START)
							game_set_status(currgame, game_status_started);
						else if (status & CLIENT_STARTGAME4_STATUS_FULL)
							game_set_status(currgame, game_status_full);
						else
							game_set_status(currgame, game_status_open);
					}
					else {
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] unknown startgame4 status {} (clienttag: {})", conn_get_socket(c), status, clienttag_uint_to_str(conn_get_clienttag(c)));
					}

				}
				else if ((status & CLIENT_STARTGAME4_STATUSMASK_INIT_VALID) == status) {
					/*valid creation status would be:
					   0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13, 0x80, 0x81, 0x82, 0x83 */

					t_game_type gtype;
					bool allow_create_custom = false;
					t_game *game;

					gtype = bngtype_to_gtype(conn_get_clienttag(c), bngtype);
					if ((gtype == game_type_ladder && account_get_auth_createladdergame(conn_get_account(c)) == 0) || (gtype != game_type_ladder && account_get_auth_createnormalgame(conn_get_account(c)) == 0))
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] game start for \"{}\" refused (no authority)", conn_get_socket(c), conn_get_username(c));
					else 
					{
						//find is there any existing game with same name and allow the host to create game
						// with same name only when another game is already started or already done
						if ((!(game = gamelist_find_game_available(gamename, conn_get_clienttag(c), game_type_all))) &&
							(conn_set_game(c, gamename, gamepass, gameinfo, gtype, STARTVER_GW4) == 0)) 
						{
							game_set_option(conn_get_game(c), bngoption_to_goption(conn_get_clienttag(c), gtype, option));
							if (status & CLIENT_STARTGAME4_STATUS_PRIVATE)
								game_set_flag(conn_get_game(c), game_flag_private);
							if (status & CLIENT_STARTGAME4_STATUS_FULL)
								game_set_status(conn_get_game(c), game_status_full);
							if (bngtype == CLIENT_GAMELISTREQ_LOADED) /* PELISH: seems strange but it is really needed for loaded games */
								game_set_status(conn_get_game(c), game_status_loaded);
							//FIXME: still need special handling for status disc-is-loss and replay

						}
					}
				}
				else
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] client tried to set game status 0x{:x} to unexistent game (clienttag: {})", conn_get_socket(c), status, clienttag_uint_to_str(conn_get_clienttag(c)));
			}

			if ((rpacket = packet_create(packet_class_bnet))) {
				packet_set_size(rpacket, sizeof(t_server_startgame4_ack));
				packet_set_type(rpacket, SERVER_STARTGAME4_ACK);

				if (conn_get_game(c))
					bn_int_set(&rpacket->u.server_startgame4_ack.reply, SERVER_STARTGAME4_ACK_OK);
				else
					bn_int_set(&rpacket->u.server_startgame4_ack.reply, SERVER_STARTGAME4_ACK_NO);
				if (pvpgn_v3_send_startgame4ack(c,
				        bn_int_get(rpacket->u.server_startgame4_ack.reply)) == 1) {
					packet_del_ref(rpacket);
				} else {
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
				}
			}

			/* First, send an ECHO_REQ */
			if ((rpacket = packet_create(packet_class_bnet))) {
				packet_set_size(rpacket, sizeof(t_server_echoreq));
				packet_set_type(rpacket, SERVER_ECHOREQ);
				bn_int_set(&rpacket->u.server_echoreq.ticks, get_ticks());
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			return 0;
		}

		int _client_closegame(t_connection * c, t_packet const *const packet)
		{
			t_game *game;

			eventlog(eventlog_level_info, __FUNCTION__, "[{}] client closing game", conn_get_socket(c));
			if (packet_get_type(packet) == CLIENT_CLOSEGAME2 || ((conn_get_clienttag(c) != CLIENTTAG_WARCRAFT3_UINT) && (conn_get_clienttag(c) != CLIENTTAG_WAR3XP_UINT)))
				conn_set_game(c, NULL, NULL, NULL, game_type_none, 0);
			else if ((game = conn_get_game(c)))
				game_set_status(game, game_status_started);

			return 0;
		}

		int _client_gamereport(t_connection * c, t_packet const *const packet)
		{
			if (packet_get_size(packet) < sizeof(t_client_game_report)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad GAME_REPORT packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_game_report), packet_get_size(packet));
				return -1;
			}

			{
				t_account *my_account;
				t_account *other_account;
				t_game *game;
				unsigned int player_count;
				unsigned int i, s;
				t_client_game_report_result const *result_data;
				unsigned int result_off;
				t_game_result result;
				char const *player;
				unsigned int player_off;
				t_game_result *results;

				player_count = bn_int_get(packet->u.client_gamerep.count);

				if (!(game = conn_get_game(c))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got GAME_REPORT when not in a game for user \"{}\"", conn_get_socket(c), conn_get_username(c));
					return -1;
				}

				eventlog(eventlog_level_info, __FUNCTION__, "[{}] CLIENT_GAME_REPORT: {} ({} players)", conn_get_socket(c), conn_get_username(c), player_count);
				// Observation-only: structured-log the GAME_REPORT entry.
				(void)pvpgn_v3_gamereport(c, conn_get_username(c), player_count);
				my_account = conn_get_account(c);

				results = new t_game_result[game_get_count(game)]{};

				for (i = 0; i < game_get_count(game); i++)
					results[i] = game_result_none;

				for (i = 0, result_off = sizeof(t_client_game_report), player_off = sizeof(t_client_game_report)+player_count * sizeof(t_client_game_report_result); i < player_count; i++, result_off += sizeof(t_client_game_report_result), player_off += std::strlen(player) + 1) {
					/* PELISH: Fixme - Can this crash server (NULL pointer dereferencing)?? */
					if (!(result_data = (const t_client_game_report_result*)packet_get_data_const(packet, result_off, sizeof(t_client_game_report_result)))) {
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] got corrupt GAME_REPORT packet (missing results {}-{})", conn_get_socket(c), i + 1, player_count);
						break;
					}
					if (!(player = packet_get_str_const(packet, player_off, MAX_USERNAME_LEN))) {
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] got corrupt GAME_REPORT packet (missing players {}-{})", conn_get_socket(c), i + 1, player_count);
						break;
					}

					if (player[0] == '\0')	/* empty slots have empty player name */
						continue;

					if (i >= game_get_count(game)) {
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] got more results than the game had players - ignoring extra results", conn_get_socket(c));
						break;
					}

					if (!(other_account = accountlist_find_account(player))) {
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] got GAME_REPORT with unknown player \"{}\"", conn_get_socket(c), player);
						break;
					}

					// as player position in game structure and in game report might differ,
					// search for right position
					for (s = 0; s < game_get_count(game); s++)
					{
						if (game_get_player(game, s) == other_account) break;
					}

					if (s < game_get_count(game))
					{
						result = bngresult_to_gresult(bn_int_get(result_data->result));
						results[s] = result;
						eventlog(eventlog_level_debug, __FUNCTION__, "[{}] got player {} (\"{}\") result {}", conn_get_socket(c), i, player, game_result_get_str(result));
					}
					else
					{
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] got GAME_REPORT for non-participating player \"{}\"", conn_get_socket(c), player);
					}


				}

				if (i == player_count) {	/* if everything checked out... */
					char const *head;
					char const *body;

					if (!(head = packet_get_str_const(packet, player_off, MAX_GAMEREP_HEAD_STR)))
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] got GAME_REPORT with missing or too long report head", conn_get_socket(c));
					else {
						player_off += std::strlen(head) + 1;
						if (!(body = packet_get_str_const(packet, player_off, MAX_GAMEREP_BODY_STR)))
							eventlog(eventlog_level_error, __FUNCTION__, "[{}] got GAME_REPORT with missing or too ling report body", conn_get_socket(c));
						else
							game_set_report(game, my_account, head, body);
					}
				}

				if (game_set_reported_results(game, my_account, results) < 0)
					delete[] results;

				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] finished parsing result... now leaving game", conn_get_socket(c));
				conn_set_game(c, NULL, NULL, NULL, game_type_none, 0);
			}

			return 0;
		}


}} // namespace pvpgn::bnetd
