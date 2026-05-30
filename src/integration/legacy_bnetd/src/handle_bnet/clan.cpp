// =====================================================================
// Auto-split from handle_bnet_link.cpp by scripts/dev/split_handle_bnet.py
// See plans/15-large-file-decomposition-detail.md for rationale.
// =====================================================================
#include "handle_bnet_internal.h"

namespace pvpgn { namespace bnetd {

		int _client_claninforeq(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_clan_dispatch(c, "claninforeq");
			t_packet *rpacket;
			int count;
			char const *username;
			t_account *account;
			t_clanmember *clanmember;
			t_clan *clan;
			t_clantag clantag1, clantag2;

			if (packet_get_size(packet) < sizeof(t_client_claninforeq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLANINFOREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_claninforeq), packet_get_size(packet));
				return -1;
			}

			count = bn_int_get(packet->u.client_claninforeq.count);
			clantag1 = bn_int_get(packet->u.client_claninforeq.clantag);
			clan = NULL;

			if (!(username = packet_get_str_const(packet, sizeof(t_client_claninforeq), MAX_USERNAME_LEN))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLANINFOREQ (missing or too long username)", conn_get_socket(c));
				return -1;
			}

			if (!(account = accountlist_find_account(username))) {
				eventlog(eventlog_level_error, __FUNCTION__, "requested claninfo for non-existant account");
				return -1;
			}

			if ((clanmember = account_get_clanmember(account)) && (clan = clanmember_get_clan(clanmember)))
				clantag2 = clan_get_clantag(clan);
			else
				clantag2 = 0;

			{
				int v3rc = pvpgn_v3_send_claninforeply(
					c,
					static_cast<unsigned int>(count),
					(clantag1 == clantag2) ? 0u : 1u,
					(clantag1 == clantag2) ? clan_get_name(clan) : nullptr,
					(clantag1 == clantag2) ? static_cast<unsigned int>(clanmember_get_status(clanmember)) : 0u,
					(clantag1 == clantag2) ? static_cast<unsigned int>(clanmember_get_join_time(clanmember)) : 0u);
				if (v3rc == 1) {
					return 0;
				}
			}
			if ((rpacket = packet_create(packet_class_bnet))) {
				packet_set_size(rpacket, sizeof(t_server_claninforeply));
				packet_set_type(rpacket, SERVER_CLANINFOREPLY);
				bn_int_set(&rpacket->u.server_profilereply.count, count);
				if (clantag1 == clantag2) {
					t_bnettime bn_time;
					bn_long ltime;

					bn_byte_set(&rpacket->u.server_claninforeply.fail, 0);

					packet_append_string(rpacket, clan_get_name(clan));
					char status = clanmember_get_status(clanmember);
					packet_append_data(rpacket, &status, 1);
					std::time_t temp = clanmember_get_join_time(clanmember);
					bn_time = time_to_bnettime(temp, 0);
					bn_time = bnettime_add_tzbias(bn_time, -conn_get_tzbias(c));
					bnettime_to_bn_long(bn_time, &ltime);
					packet_append_data(rpacket, &ltime, 8);
				}
				else
					bn_byte_set(&rpacket->u.server_claninforeply.fail, 1);


				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			return 0;
		}

		int _client_clanmemberlistreq(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_clan_dispatch(c, "clanmemberlistreq");
			if (packet_get_size(packet) < sizeof(t_client_clanmemberlist_req)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLANMEMBERLIST_REQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_clanmemberlist_req), packet_get_size(packet));
				return -1;
			}

			clan_send_memberlist(c, packet);
			return 0;
		}

		int _client_clan_motdreq(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_clan_dispatch(c, "clan_motdreq");
			if (packet_get_size(packet) < sizeof(t_client_clan_motdreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLAN_MOTDREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_clan_motdreq), packet_get_size(packet));
				return -1;
			}

			clan_send_motd_reply(c, packet);
			return 0;
		}

		int _client_clan_motdchg(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_clan_dispatch(c, "clan_motdchg");
			if (packet_get_size(packet) < sizeof(t_client_clan_motdreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLAN_MOTDCHGREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_clan_motdreq), packet_get_size(packet));
				return -1;
			}

			clan_save_motd_chg(c, packet);
			return 0;
		}

		int _client_clan_disbandreq(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_clan_dispatch(c, "clan_disbandreq");
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_clan_disbandreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLAN_DISBANDREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_clan_disbandreq), packet_get_size(packet));
				return -1;
			}

			if ((rpacket = packet_create(packet_class_bnet))) {
				t_clan *clan;
				t_clanmember *member;
				t_account *account;

				packet_set_size(rpacket, sizeof(t_server_clan_disbandreply));
				packet_set_type(rpacket, SERVER_CLAN_DISBANDREPLY);
				bn_int_set(&rpacket->u.server_clan_disbandreply.count, bn_int_get(packet->u.client_clan_disbandreq.count));

				if (!((account = conn_get_account(c)) && (clan = account_get_clan(account)) && (member = account_get_clanmember(account)) && (clanmember_get_status(member) >= CLAN_CHIEFTAIN))) {
					eventlog(eventlog_level_warn, __FUNCTION__, "[{}] got suspicious CLAN_DISBANDREQ packet (request without required privileges)", conn_get_socket(c));
					bn_byte_set(&rpacket->u.server_clan_disbandreply.result, CLAN_RESPONSE_NOT_AUTHORIZED);
					if (pvpgn_v3_send_clandisbandreply(c,
					        bn_int_get(rpacket->u.server_clan_disbandreply.count),
					        bn_byte_get(rpacket->u.server_clan_disbandreply.result)) == 1) {
						packet_del_ref(rpacket);
					} else {
						conn_push_outqueue(c, rpacket);
						packet_del_ref(rpacket);
					}
				}
				else if ((clanlist_remove_clan(clan) == 0) && (clan_remove(clan_get_clantag(clan)) == 0)) {
					bn_byte_set(&rpacket->u.server_clan_disbandreply.result, CLAN_RESPONSE_SUCCESS);
					clan_close_status_window_on_disband(clan);
					clan_send_packet_to_online_members(clan, rpacket);
					packet_del_ref(rpacket);
					clan_destroy(clan);
				}
				else {
					bn_byte_set(&rpacket->u.server_clan_disbandreply.result, CLAN_RESPONSE_FAIL);
					if (pvpgn_v3_send_clandisbandreply(c,
					        bn_int_get(rpacket->u.server_clan_disbandreply.count),
					        bn_byte_get(rpacket->u.server_clan_disbandreply.result)) == 1) {
						packet_del_ref(rpacket);
					} else {
						conn_push_outqueue(c, rpacket);
						packet_del_ref(rpacket);
					}
				}
			}

			return 0;
		}

		int _client_clan_createreq(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_clan_dispatch(c, "clan_createreq");
			if (packet_get_size(packet) < sizeof(t_client_clan_createreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLAN_INFOREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_clan_createreq), packet_get_size(packet));
				return -1;
			}

			clan_get_possible_member(c, packet);

			return 0;
		}

		int _client_clan_createinvitereq(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_clan_dispatch(c, "clan_createinvitereq");
			t_packet *rpacket;
			unsigned size;
			const char *clanname;
			const char *username;
			t_clantag clantag;
			unsigned offset;
			t_clan *clan;

			if ((size = packet_get_size(packet)) < sizeof(t_client_clan_createinvitereq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLAN_CREATEINVITEREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_clan_createinvitereq), packet_get_size(packet));
				return -1;
			}
			offset = sizeof(t_client_clan_createinvitereq);

			if (!(clanname = packet_get_str_const(packet, offset, CLAN_NAME_MAX))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLAN_CREATEINVITEREQ packet (missing clanname)", conn_get_socket(c));
				return -1;
			}
			offset += (std::strlen(clanname) + 1);

			if (packet_get_size(packet) < offset + 4) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLAN_CREATEINVITEREQ packet (missing clantag)", conn_get_socket(c));
				return -1;
			}
			clantag = *((int *)packet_get_data_const(packet, offset, 4));
			offset += 4;

			if ((rpacket = packet_create(packet_class_bnet))) {
				if ((clan = clan_create(conn_get_account(c), clantag, clanname, NULL)) && clanlist_add_clan(clan)) {
					char membercount;
					if (packet_get_size(packet) < offset + 1) {
						eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLAN_CREATEINVITEREQ packet (missing membercount)", conn_get_socket(c));
						packet_del_ref(rpacket);
						return -1;
					}
					membercount = *((char *)packet_get_data_const(packet, offset, 1));
					clan_set_created(clan, -membercount);                               /* FIXME: We should also check if membercount == count of names on end of this packet */
					packet_set_size(rpacket, sizeof(t_server_clan_createinvitereq));
					packet_set_type(rpacket, SERVER_CLAN_CREATEINVITEREQ);
					bn_int_set(&rpacket->u.server_clan_createinvitereq.count, bn_int_get(packet->u.client_clan_createinvitereq.count));
					bn_int_set(&rpacket->u.server_clan_createinvitereq.clantag, clantag);
					packet_append_string(rpacket, clanname);
					packet_append_string(rpacket, conn_get_username(c));
					packet_append_data(rpacket, packet_get_data_const(packet, offset, size - offset), size - offset); /* Pelish: we will send bad packet if we got bad packet... */
					offset++;
					do {
						username = packet_get_str_const(packet, offset, MAX_USERNAME_LEN);
						if (username) {
							t_connection *conn;
							offset += (std::strlen(username) + 1);
							if ((conn = connlist_find_connection_by_accountname(username)) != NULL) {
								t_clanmember *clanmember;
								if (prefs_v3::clan_newer_time() > 0) {
									clanmember = clan_add_member(clan, conn_get_account(conn), CLAN_NEW);
									clanmember_set_fullmember(clanmember, 1);      /* FIXME: do only this here and no clan_add_member() */
								}
								else {
									clanmember = clan_add_member(clan, conn_get_account(conn), CLAN_PEON);
									clanmember_set_fullmember(clanmember, 1);      /* FIXME: do only this here and no clan_add_member() */
								}
								if (pvpgn_v3_send_clancreateinviteforward(
								        conn,
								        bn_int_get(rpacket->u.server_clan_createinvitereq.count),
								        bn_int_get(rpacket->u.server_clan_createinvitereq.clantag),
								        clanname,
								        conn_get_username(c),
								        nullptr,
								        0) != 1) {
									conn_push_outqueue(conn, rpacket);
								}
							}
						}
					} while (username && (offset < size));
				}
				else {
					packet_set_size(rpacket, sizeof(t_server_clan_createinvitereply));
					packet_set_type(rpacket, SERVER_CLAN_CREATEINVITEREPLY);
					bn_int_set(&rpacket->u.server_clan_createinvitereply.count, bn_int_get(packet->u.client_clan_createinvitereply.count));
					bn_byte_set(&rpacket->u.server_clan_createinvitereply.status, 0);
					(void)pvpgn_v3_send_clancreateinvitereply(
								 c,
								 bn_int_get(rpacket->u.server_clan_createinvitereply.count),
								 0u,
								 "",
								 bn_byte_get(rpacket->u.server_clan_createinvitereply.status));
				}
				packet_del_ref(rpacket);
			}

			return 0;
		}

		int _client_clan_createinvitereply(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_clan_dispatch(c, "clan_createinvitereply");
			t_packet *rpacket;
			t_connection *conn;
			t_clan *clan;
			const char *username;
			char status;

			if (packet_get_size(packet) < sizeof(t_client_clan_createinvitereply)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLAN_CREATEINVITEREPLY packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_clan_createinvitereq), packet_get_size(packet));
				return -1;
			}
			std::size_t offset = sizeof(t_client_clan_createinvitereply);
			username = packet_get_str_const(packet, offset, MAX_USERNAME_LEN);
			if (!username)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLAN_CREATEINVITEREPLY packet (bad username)", conn_get_socket(c));
				return -1;
			}
			offset += (std::strlen(username) + 1);
			if (packet_get_size(packet) < offset + 1) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLAN_CREATEINVITEREPLY packet (mising status)", conn_get_socket(c));
				return -1;
			}
			status = *((char *)packet_get_data_const(packet, offset, 1));
			if ((conn = connlist_find_connection_by_accountname(username)) == NULL)
				return -1;
			if ((clan = account_get_creating_clan(conn_get_account(conn))) == NULL)
				return -1;
			if ((status != CLAN_RESPONSE_ACCEPT) && (rpacket = packet_create(packet_class_bnet))) {
				packet_set_size(rpacket, sizeof(t_server_clan_createinvitereply));
				packet_set_type(rpacket, SERVER_CLAN_CREATEINVITEREPLY);
				bn_int_set(&rpacket->u.server_clan_createinvitereply.count, bn_int_get(packet->u.client_clan_createinvitereply.count));
				bn_byte_set(&rpacket->u.server_clan_createinvitereply.status, status);
				packet_append_string(rpacket, conn_get_username(c));
				if (pvpgn_v3_send_clancreateinvitereply(
				        conn,
				        bn_int_get(rpacket->u.server_clan_createinvitereply.count),
				        0u,
				        conn_get_username(c),
				        bn_byte_get(rpacket->u.server_clan_createinvitereply.status)) == 1) {
					packet_del_ref(rpacket);
				} else {
					conn_push_outqueue(conn, rpacket);
					packet_del_ref(rpacket);
				}
				if (clan) {
					clanlist_remove_clan(clan);
					clan_destroy(clan);
				}
			}
			else {
				int created = clan_get_created(clan);
				if (created > 0) {
					eventlog(eventlog_level_error, __FUNCTION__, "clan {} has already been created", clan_get_name(clan));
					return 0;
				}
				created++;
				if ((created >= 0) && (rpacket = packet_create(packet_class_bnet))) {
					clan_set_created(clan, 1);
					clan_set_creation_time(clan, std::time(NULL));
					packet_set_size(rpacket, sizeof(t_server_clan_createinvitereply));
					packet_set_type(rpacket, SERVER_CLAN_CREATEINVITEREPLY);
					bn_int_set(&rpacket->u.server_clan_createinvitereply.count, bn_int_get(packet->u.client_clan_createinvitereply.count));
					bn_byte_set(&rpacket->u.server_clan_createinvitereply.status, CLAN_RESPONSE_SUCCESS);
					packet_append_string(rpacket, "");
					if (pvpgn_v3_send_clancreateinvitereply(
					        conn,
					        bn_int_get(rpacket->u.server_clan_createinvitereply.count),
					        0u,
					        "",
					        CLAN_RESPONSE_SUCCESS) == 1) {
						packet_del_ref(rpacket);
					} else {
						conn_push_outqueue(conn, rpacket);
						packet_del_ref(rpacket);
					}
					clan_send_status_window_on_create(clan);
					clan_save(clan);
				}
				else
					clan_set_created(clan, created);
			}
			return 0;
		}

		int _client_clanmember_rankupdatereq(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_clan_dispatch(c, "clanmember_rankupdatereq");
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_clanmember_rankupdate_req)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLANMEMBER_RANKUPDATE_REQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_clanmember_rankupdate_req), packet_get_size(packet));
				return -1;
			}

			if ((rpacket = packet_create(packet_class_bnet)) != NULL)
			{
				std::size_t offset = sizeof(t_client_clanmember_rankupdate_req);
				char status;
				t_clan *clan;
				t_clanmember *dest_member;
				t_clanmember *member;
				t_account *account;

				packet_set_size(rpacket, sizeof(t_server_clanmember_rankupdate_reply));
				packet_set_type(rpacket, SERVER_CLANMEMBER_RANKUPDATE_REPLY);
				bn_int_set(&rpacket->u.server_clanmember_rankupdate_reply.count,
					bn_int_get(packet->u.client_clanmember_rankupdate_req.count));
				const char* const username = packet_get_str_const(packet, offset, MAX_USERNAME_LEN);
				if (!username)
				{
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] Could not retrieve username from CLANMEMBER_RANKUPDATE_REQ packet", conn_get_socket(c));
					packet_del_ref(rpacket);
					return -1;
				}

				offset += (std::strlen(username) + 1);
				if (packet_get_size(packet) < offset + 1)
				{
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLANMEMBER_RANKUPDATE_REQ packet (mising status)", conn_get_socket(c));
					packet_del_ref(rpacket);
					return -1;
				}
				status = *((char *)packet_get_data_const(packet, offset, 1));

				account = (conn_get_account(c));

				if ((clan = account_get_clan(account)) && (member = clan_find_member(clan, account))
					&& (dest_member = clan_find_member_by_name(clan, username)) && (member != dest_member)) {
					if ((status < CLAN_PEON) || (status > CLAN_SHAMAN)) {
						/* PELISH: CLAN_NEW can not be promoted to anything
						 * and also noone can be promoted to CLAN_CHIEFTAIN */
						DEBUG1("trying to change to bad status {}", status);
						bn_byte_set(&rpacket->u.server_clanmember_rankupdate_reply.result, SERVER_CLANMEMBER_RANKUPDATE_FAILED);
					}
					else if ((((clanmember_get_status(member) == CLAN_SHAMAN) && (status < CLAN_SHAMAN) && (clanmember_get_status(dest_member) < CLAN_SHAMAN)) ||
						(clanmember_get_status(member) == CLAN_CHIEFTAIN)) &&
						(clanmember_set_status(dest_member, status) == 0)) {
						bn_byte_set(&rpacket->u.server_clanmember_rankupdate_reply.result, SERVER_CLANMEMBER_RANKUPDATE_SUCCESS);
						clanmember_on_change_status(dest_member);
					}
					else {
						bn_byte_set(&rpacket->u.server_clanmember_rankupdate_reply.result, SERVER_CLANMEMBER_RANKUPDATE_FAILED);
					}
				}
				else {
					bn_byte_set(&rpacket->u.server_clanmember_rankupdate_reply.result, SERVER_CLANMEMBER_RANKUPDATE_FAILED);
				}
				{
					unsigned int  count_v3  = static_cast<unsigned int>(
						bn_int_get(rpacket->u.server_clanmember_rankupdate_reply.count));
					unsigned char result_v3 = static_cast<unsigned char>(
						bn_byte_get(rpacket->u.server_clanmember_rankupdate_reply.result));
					if (pvpgn_v3_send_clanmember_rankupdate_reply(c, count_v3, result_v3) == 1) {
						packet_del_ref(rpacket);
						return 0;
					}
				}
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			return 0;
		}

		int _client_clanmember_removereq(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_clan_dispatch(c, "clanmember_removereq");
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_clanmember_remove_req)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLANMEMBER_REMOVE_REQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_clanmember_remove_req), packet_get_size(packet));
				return -1;
			}

			if ((rpacket = packet_create(packet_class_bnet)) != NULL) {
				t_account *acc;
				t_clan *clan;
				const char *username;
				t_clanmember *member;
				t_connection *dest_conn;
				packet_set_size(rpacket, sizeof(t_server_clanmember_remove_reply));
				packet_set_type(rpacket, SERVER_CLANMEMBER_REMOVE_REPLY);
				bn_int_set(&rpacket->u.server_clanmember_remove_reply.count,
					bn_int_get(packet->u.client_clanmember_remove_req.count));
				username = packet_get_str_const(packet, sizeof(t_client_clanmember_remove_req), MAX_USERNAME_LEN);
				bn_byte_set(&rpacket->u.server_clanmember_remove_reply.result,
					SERVER_CLANMEMBER_REMOVE_FAILED); // initially presume it failed

				if ((acc = conn_get_account(c)) && (clan = account_get_clan(acc)) && (member = clan_find_member_by_name(clan, username))) {
					dest_conn = clanmember_get_conn(member);
					if (clan_remove_member(clan, member) == 0) {
						t_packet *rpacket2;
						if (dest_conn) {
							clan_close_status_window(dest_conn);
							conn_update_w3_playerinfo(dest_conn);
							channel_rejoin(dest_conn);
						}
						if ((rpacket2 = packet_create(packet_class_bnet)) != NULL) {
							packet_set_size(rpacket2, sizeof(t_server_clanmember_removed_notify));
							packet_set_type(rpacket2, SERVER_CLANMEMBER_REMOVED_NOTIFY);
							packet_append_string(rpacket2, username);
							clan_send_packet_to_online_members(clan, rpacket2);
							packet_del_ref(rpacket2);
						}
						bn_byte_set(&rpacket->u.server_clanmember_remove_reply.result,
							SERVER_CLANMEMBER_REMOVE_SUCCESS);
					}
				}
				{
					unsigned int  count_v3  = static_cast<unsigned int>(
						bn_int_get(rpacket->u.server_clanmember_remove_reply.count));
					unsigned char result_v3 = static_cast<unsigned char>(
						bn_byte_get(rpacket->u.server_clanmember_remove_reply.result));
					if (pvpgn_v3_send_clanmember_remove_reply(c, count_v3, result_v3) == 1) {
						packet_del_ref(rpacket);
						return 0;
					}
				}
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			return 0;
		}

		int _client_clan_membernewchiefreq(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_clan_dispatch(c, "clan_membernewchiefreq");
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_clan_membernewchiefreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLIENT_CLAN_MEMBERNEWCHIEFREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_clan_createreq), packet_get_size(packet));
				return -1;
			}

			if ((rpacket = packet_create(packet_class_bnet)) != NULL) {
				t_account *acc;
				t_clan *clan;
				t_clanmember *oldmember;
				t_clanmember *newmember;
				const char *username;
				packet_set_size(rpacket, sizeof(t_server_clan_membernewchiefreply));
				packet_set_type(rpacket, SERVER_CLAN_MEMBERNEWCHIEFREPLY);
				bn_int_set(&rpacket->u.server_clan_membernewchiefreply.count, bn_int_get(packet->u.client_clan_membernewchiefreq.count));
				username = packet_get_str_const(packet, sizeof(t_client_clan_membernewchiefreq), MAX_USERNAME_LEN);
				if ((acc = conn_get_account(c)) && (oldmember = account_get_clanmember(acc)) && (clanmember_get_status(oldmember) == CLAN_CHIEFTAIN) && (clan = clanmember_get_clan(oldmember)) && (newmember = clan_find_member_by_name(clan, username)) && (clanmember_set_status(oldmember, CLAN_GRUNT) == 0) && (clanmember_set_status(newmember, CLAN_CHIEFTAIN) == 0)) {
					clanmember_on_change_status(oldmember);
					clanmember_on_change_status(newmember);
					bn_byte_set(&rpacket->u.server_clan_membernewchiefreply.result, SERVER_CLAN_MEMBERNEWCHIEFREPLY_SUCCESS);
					clan_send_packet_to_online_members(clan, rpacket);
					packet_del_ref(rpacket);
				}
				else {
					if (pvpgn_v3_send_clan_membernewchief_reply(c,
							static_cast<unsigned int>(bn_int_get(rpacket->u.server_clan_membernewchiefreply.count)),
							static_cast<unsigned char>(SERVER_CLAN_MEMBERNEWCHIEFREPLY_FAILED)) == 1) {
						packet_del_ref(rpacket);
					} else
					{
						bn_byte_set(&rpacket->u.server_clan_membernewchiefreply.result, SERVER_CLAN_MEMBERNEWCHIEFREPLY_FAILED);
						conn_push_outqueue(c, rpacket);
						packet_del_ref(rpacket);
					}
				}
			}

			return 0;
		}

		int _client_clan_invitereq(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_clan_dispatch(c, "clan_invitereq");
			t_packet *rpacket;
			t_account *account;
			t_clan *clan;
			t_clanmember *member;
			t_clantag clantag;
			const char *username;
			t_connection *conn;
			t_account *conn_account;
			char response_code;

			if (packet_get_size(packet) < sizeof(t_client_clan_invitereq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLAN_INVITEREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_clan_invitereq), packet_get_size(packet));
				return -1;
			}

			if (!(username = packet_get_str_const(packet, sizeof(t_client_clan_invitereq), MAX_USERNAME_LEN))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLAN_INVITEREQ packet (missing or too long username)", conn_get_socket(c));
				return -1;
			}

			if (rpacket = packet_create(packet_class_bnet)) {

				// user not authorized
				if (!((account = conn_get_account(c)) &&
					(clan = account_get_clan(account)) &&
					(member = account_get_clanmember(account)) &&
					(clanmember_get_status(member) >= CLAN_SHAMAN) &&
					(clantag = clan_get_clantag(clan)))) {
					eventlog(eventlog_level_warn, __FUNCTION__, "[{}] got suspicious CLAN_INVITEREQ packet (request without required privileges)", conn_get_socket(c));
					response_code = CLAN_RESPONSE_NOT_AUTHORIZED;
				}
				else {

					// target user not online
					if (!((conn = connlist_find_connection_by_accountname(username)) &&
						(conn_account = conn_get_account(conn)))) {
						response_code = CLAN_RESPONSE_NOT_FOUND;

						// target user already in a clan or creating a clan
					}
					else if (account_get_clanmember_forced(conn_get_account(conn))) {
						response_code = CLAN_RESPONSE_NOT_FOUND;

						// clan allready ful
					}
					else if (clan_get_member_count(clan) >= prefs_v3::clan_max_members()) {
						response_code = CLAN_RESPONSE_CLAN_FULL;

						// valid invitereq
					}
					else {
						if (prefs_v3::clan_newer_time() > 0) {
							clan_add_member(clan, conn_account, CLAN_NEW);
						}
						else {
							clan_add_member(clan, conn_account, CLAN_PEON);
						}
						packet_set_size(rpacket, sizeof(t_server_clan_invitereq));
						packet_set_type(rpacket, SERVER_CLAN_INVITEREQ);
						bn_int_set(&rpacket->u.server_clan_invitereq.count, bn_int_get(packet->u.client_clan_invitereq.count));
						bn_int_set(&rpacket->u.server_clan_invitereq.clantag, clantag);
						packet_append_string(rpacket, clan_get_name(clan));
						packet_append_string(rpacket, conn_get_username(c));
						conn_push_outqueue(conn, rpacket);
						packet_del_ref(rpacket);
						return 0;
					}
				}

				packet_set_size(rpacket, sizeof(t_server_clan_invitereply));
				packet_set_type(rpacket, SERVER_CLAN_INVITEREPLY);
				bn_byte_set(&rpacket->u.server_clan_invitereply.result, response_code);
				bn_int_set(&rpacket->u.server_clan_invitereply.count, bn_int_get(packet->u.client_clan_invitereq.count));

				if (pvpgn_v3_send_clan_invitereply(c,
						static_cast<unsigned int>(bn_int_get(packet->u.client_clan_invitereq.count)),
						static_cast<unsigned char>(response_code)) == 1) {
					packet_del_ref(rpacket);
					return 0;
				}
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			return 0;
		}

		int _client_clan_invitereply(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_clan_dispatch(c, "clan_invitereply");
			t_packet *rpacket;
			t_account *acc;
			t_clan *clan;
			t_clanmember *member;
			const char *username;
			t_connection *conn;
			t_account *conn_account;
			t_clan *conn_clan;
			t_clanmember *conn_member;
			char status;

			if (packet_get_size(packet) < sizeof(t_client_clan_invitereply)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLAN_INVITEREPLY packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_clan_createreq), packet_get_size(packet));
				return -1;
			}
			std::size_t offset = sizeof(t_client_clan_invitereply);
			if (!(username = packet_get_str_const(packet, offset, MAX_USERNAME_LEN))) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLAN_INVITEREPLY packet (missing username)", conn_get_socket(c));
				return -1;
			}
			offset += (std::strlen(username) + 1);
			if (packet_get_size(packet) < offset + 1) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CLAN_INVITEREPLY packet (mising status)", conn_get_socket(c));
				return -1;
			}
			status = *((char *)packet_get_data_const(packet, offset, 1));

			// reply without prior request
			if (!((acc = conn_get_account(c)) &&
				(member = account_get_clanmember_forced(acc)) &&
				(clan = clanmember_get_clan(member)) &&
				(clanmember_get_fullmember(member) == 0))) {
				eventlog(eventlog_level_warn, __FUNCTION__, "[{}] got suspicious CLAN_INVITEREPLY packet (reply without prior request)", conn_get_socket(c));
				return -1;

				// invalid inviter
			}
			else if (!((conn = connlist_find_connection_by_accountname(username)) &&
				(conn_account = conn_get_account(conn)) &&
				(conn_clan = account_get_clan(conn_account)) &&
				(conn_member = account_get_clanmember(conn_account)) &&
				(clanmember_get_status(conn_member) >= CLAN_SHAMAN) &&
				(clan_get_clantag(clan) == clan_get_clantag(conn_clan)))) {
				eventlog(eventlog_level_warn, __FUNCTION__, "[{}] got suspicious CLAN_INVITEREPLY packet (invalid inviter)", conn_get_socket(c));
				return -1;
			}

			if (rpacket = packet_create(packet_class_bnet)) {
				packet_set_size(rpacket, sizeof(t_server_clan_invitereply));
				packet_set_type(rpacket, SERVER_CLAN_INVITEREPLY);
				bn_int_set(&rpacket->u.server_clan_invitereply.count, bn_int_get(packet->u.client_clan_invitereply.count));

				if (status != CLAN_RESPONSE_ACCEPT) {
					clan_remove_member(clan, member);
					bn_byte_set(&rpacket->u.server_clan_invitereply.result, status);
				}
				else {
					if (clan_get_member_count(clan) >= prefs_v3::clan_max_members()) {
						clan_remove_member(clan, member);
						bn_byte_set(&rpacket->u.server_clan_invitereply.result, CLAN_RESPONSE_CLAN_FULL);
					}
					else
					{
						clanmember_set_fullmember(member, 1);
						if (conn_get_channel(c))
						{
							conn_update_w3_playerinfo(c);
							channel_set_userflags(c);

							std::string channelname("Clan " + std::string(clantag_to_str(clan_get_clantag(clan))));
							if (conn_set_channel(c, channelname.c_str()) < 0)
							{
								conn_set_channel(c, CHANNEL_NAME_BANNED);	/* should not fail */
							}
							clanmember_set_online(c);
						}
						clan_send_status_window(c);
						bn_byte_set(&rpacket->u.server_clan_invitereply.result, CLAN_RESPONSE_SUCCESS);
					}
				}
				{
					unsigned int  count_v3  = static_cast<unsigned int>(
						bn_int_get(rpacket->u.server_clan_invitereply.count));
					unsigned char result_v3 = static_cast<unsigned char>(
						bn_byte_get(rpacket->u.server_clan_invitereply.result));
					if (pvpgn_v3_send_clan_invitereply(conn, count_v3, result_v3) == 1) {
						packet_del_ref(rpacket);
						return 0;
					}
				}
			}

			conn_push_outqueue(conn, rpacket);
			packet_del_ref(rpacket);

			return 0;
		}


}} // namespace pvpgn::bnetd
