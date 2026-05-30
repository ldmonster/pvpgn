// =====================================================================
// Auto-split from handle_bnet_link.cpp by scripts/dev/split_handle_bnet.py
// See plans/15-large-file-decomposition-detail.md for rationale.
// =====================================================================
#include "handle_bnet_internal.h"

namespace pvpgn { namespace bnetd {

		int _client_realmlistreq(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_realm_dispatch(c, "realmlistreq");
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_realmlistreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad REALMLISTREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_realmlistreq), packet_get_size(packet));
				return -1;
			}

			// R172.d + R188.b + R189: v3 realm-list dispatcher MANDATORY
			// under PVPGN_V3_BNETD_INTEGRATION (matches the R168.a init
			// pattern). `install_realm_list_handler()` is called
			// unconditionally in `server.cpp`; rc < 0 means startup
			// wiring is broken -- surface as error rather than silently
			// falling through to the legacy `realmlist()` loop.
			{
				int v3rc = pvpgn_v3_realm_list_apply(c, /*legacy_format=*/1);
				if (v3rc < 0) {
					eventlog(eventlog_level_error, __FUNCTION__,
					    "[{}] v3 realm-list dispatcher not installed (rc={}); rejecting REALMLISTREQ",
					    conn_get_socket(c), v3rc);
					return -1;
				}
				return 0;
			}
		}

		int _client_realmlistreq110(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_realm_dispatch(c, "realmlistreq110");
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_realmlistreq_110)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad REALMLISTREQ_110 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_realmlistreq), packet_get_size(packet));
				return -1;
			}

			// R172.d + R188.b + R189: see the matching block in
			// `_client_realmlistreq` -- v3 dispatcher is MANDATORY.
			{
				int v3rc = pvpgn_v3_realm_list_apply(c, /*legacy_format=*/0);
				if (v3rc < 0) {
					eventlog(eventlog_level_error, __FUNCTION__,
					    "[{}] v3 realm-list dispatcher not installed (rc={}); rejecting REALMLISTREQ_110",
					    conn_get_socket(c), v3rc);
					return -1;
				}
				return 0;
			}
		}

		int _client_realmjoinreq109(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_realm_dispatch(c, "realmjoinreq109");
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_realmjoinreq_109)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad REALMJOINREQ_109 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_realmjoinreq_109), packet_get_size(packet));
				return -1;
			}

			{
				char const *realmname;
				t_realm *realm;

				if (!(realmname = packet_get_str_const(packet, sizeof(t_client_realmjoinreq_109), MAX_REALMNAME_LEN))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad REALMJOINREQ_109 (missing or too long realmname)", conn_get_socket(c));
					return -1;
				}

				if ((realm = realmlist_find_realm(realmname))) {
					unsigned int salt;
					struct {
						bn_int salt;
						bn_int sessionkey;
						bn_int sessionnum;
						bn_int secret;
						bn_int passhash[5];
					} temp;
					char const *pass_str;
					t_hash secret_hash;
					t_hash passhash;
					t_realm *prev_realm;

					/* FIXME: should we only set this after they log in to the realm server? */
					prev_realm = conn_get_realm(c);
					if (prev_realm) {
						if (prev_realm != realm) {
							realm_add_player_number(realm, 1);
							realm_add_player_number(prev_realm, -1);
							conn_set_realm(c, realm);
						}
					}
					else {
						realm_add_player_number(realm, 1);
						conn_set_realm(c, realm);
					}

					if ((pass_str = account_get_pass(conn_get_account(c)))) {
						if (hash_set_str(&passhash, pass_str) == 0) {
							hash_to_bnhash((t_hash const *)&passhash, temp.passhash);
							salt = bn_int_get(packet->u.client_realmjoinreq_109.seqno);
							bn_int_set(&temp.salt, salt);
							bn_int_set(&temp.sessionkey, conn_get_sessionkey(c));
							bn_int_set(&temp.sessionnum, conn_get_sessionnum(c));
							bn_int_set(&temp.secret, conn_get_secret(c));
							bnet_hash(&secret_hash, sizeof(temp), &temp);

							{
								unsigned int v3_addr = realm_get_ip(realm);
								unsigned short v3_port = realm_get_port(realm);
								trans_net(conn_get_addr(c), &v3_addr, &v3_port);
								int v3rc = pvpgn_v3_send_realmjoinreply(
									c,
									static_cast<unsigned int>(salt),
									0u,
									0u,
									static_cast<unsigned int>(conn_get_sessionnum(c)),
									v3_addr,
									static_cast<unsigned int>(v3_port),
									0u,
									static_cast<unsigned int>(conn_get_sessionkey(c)),
									0u,
									0u,
									static_cast<unsigned int>(conn_get_clienttag(c)),
									static_cast<unsigned int>(conn_get_versionid(c)),
									0u,
									0u,
									reinterpret_cast<unsigned int const*>(&secret_hash),
									conn_get_username(c));
								if (v3rc == 1) {
									return 0;
								}
							}
							if ((rpacket = packet_create(packet_class_bnet))) {
								packet_set_size(rpacket, sizeof(t_server_realmjoinreply_109));
								packet_set_type(rpacket, SERVER_REALMJOINREPLY_109);
								bn_int_set(&rpacket->u.server_realmjoinreply_109.seqno, salt);
								bn_int_set(&rpacket->u.server_realmjoinreply_109.u1, 0x0);
								bn_short_set(&rpacket->u.server_realmjoinreply_109.u3, 0x0);	/* reg auth */
								bn_int_set(&rpacket->u.server_realmjoinreply_109.bncs_addr1, 0x0);
								bn_int_set(&rpacket->u.server_realmjoinreply_109.sessionnum, conn_get_sessionnum(c));
								{	/* trans support */
									unsigned int addr = realm_get_ip(realm);
									unsigned short port = realm_get_port(realm);
	
									trans_net(conn_get_addr(c), &addr, &port);
	
									bn_int_nset(&rpacket->u.server_realmjoinreply_109.addr, addr);
									bn_short_nset(&rpacket->u.server_realmjoinreply_109.port, port);
								}
								bn_int_set(&rpacket->u.server_realmjoinreply_109.sessionkey, conn_get_sessionkey(c));
								bn_int_set(&rpacket->u.server_realmjoinreply_109.u5, 0);
								bn_int_set(&rpacket->u.server_realmjoinreply_109.u6, 0);
								bn_int_set(&rpacket->u.server_realmjoinreply_109.bncs_addr2, 0);
								bn_int_set(&rpacket->u.server_realmjoinreply_109.u7, 0);
								bn_int_set(&rpacket->u.server_realmjoinreply_109.versionid, conn_get_versionid(c));
								bn_int_set(&rpacket->u.server_realmjoinreply_109.clienttag, conn_get_clienttag(c));
								hash_to_bnhash((t_hash const *)&secret_hash, rpacket->u.server_realmjoinreply_109.secret_hash);	/* avoid warning */
								packet_append_string(rpacket, conn_get_username(c));
								conn_push_outqueue(c, rpacket);
								packet_del_ref(rpacket);
							}
							return 0;
						}
						else
							eventlog(eventlog_level_info, __FUNCTION__, "[{}] realm join for \"{}\" failed (unable to hash password)", conn_get_socket(c), conn_get_loggeduser(c));
					}
					else {
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] realm join for \"{}\" failed (no password)", conn_get_socket(c), conn_get_loggeduser(c));
					}
				}
				else
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] could not find active realm \"{}\"", conn_get_socket(c), realmname);

				{
					static const unsigned int zero_hash[5] = {0, 0, 0, 0, 0};
					int v3rc = pvpgn_v3_send_realmjoinreply(
						c,
						static_cast<unsigned int>(bn_int_get(packet->u.client_realmjoinreq_109.seqno)),
						0u,
						0u,
						0u,
						0u,
						0u,
						0u,
						0u,
						0u,
						0u,
						0u,
						0u,
						0u,
						0u,
						zero_hash,
						"");
					if (v3rc == 1) {
						return 0;
					}
				}
				if ((rpacket = packet_create(packet_class_bnet))) {
					packet_set_size(rpacket, sizeof(t_server_realmjoinreply_109));
					packet_set_type(rpacket, SERVER_REALMJOINREPLY_109);
					bn_int_set(&rpacket->u.server_realmjoinreply_109.seqno, bn_int_get(packet->u.client_realmjoinreq_109.seqno));
					bn_int_set(&rpacket->u.server_realmjoinreply_109.u1, 0);
					bn_int_set(&rpacket->u.server_realmjoinreply_109.sessionnum, 0);
					bn_short_set(&rpacket->u.server_realmjoinreply_109.u3, 0);
					bn_int_nset(&rpacket->u.server_realmjoinreply_109.addr, 0);
					bn_short_nset(&rpacket->u.server_realmjoinreply_109.port, 0);
					bn_int_set(&rpacket->u.server_realmjoinreply_109.sessionkey, 0);
					bn_int_set(&rpacket->u.server_realmjoinreply_109.u5, 0);
					bn_int_set(&rpacket->u.server_realmjoinreply_109.u6, 0);
					bn_int_set(&rpacket->u.server_realmjoinreply_109.u7, 0);
					bn_int_set(&rpacket->u.server_realmjoinreply_109.bncs_addr1, 0);
					bn_int_set(&rpacket->u.server_realmjoinreply_109.bncs_addr2, 0);
					bn_int_set(&rpacket->u.server_realmjoinreply_109.versionid, 0);
					bn_int_set(&rpacket->u.server_realmjoinreply_109.clienttag, 0);
					bn_int_set(&rpacket->u.server_realmjoinreply_109.secret_hash[0], 0);
					bn_int_set(&rpacket->u.server_realmjoinreply_109.secret_hash[1], 0);
					bn_int_set(&rpacket->u.server_realmjoinreply_109.secret_hash[2], 0);
					bn_int_set(&rpacket->u.server_realmjoinreply_109.secret_hash[3], 0);
					bn_int_set(&rpacket->u.server_realmjoinreply_109.secret_hash[4], 0);
					packet_append_string(rpacket, "");
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
				}
			}

			return 0;
		}

		int _client_charlistreq(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_d2_character_dispatch(c, "charlistreq");
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_unknown_37)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad UNKNOWN_37 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_unknown_37), packet_get_size(packet));
				return -1;
			}
			/*
			   0x0070:                           83 80 ff ff ff ff ff 2f    t,taran,......./
			   0x0080: ff ff ff ff ff ff ff ff   ff ff 03 ff ff ff ff ff    ................
			   0x0090: ff ff ff ff ff ff ff ff   ff ff ff 07 80 80 80 80    ................
			   0x00a0: ff ff ff 00
			   */
			if ((rpacket = packet_create(packet_class_bnet))) {
				char const *charlist;
				char *temp;

				packet_set_size(rpacket, sizeof(t_server_unknown_37));
				packet_set_type(rpacket, SERVER_UNKNOWN_37);
				bn_int_set(&rpacket->u.server_unknown_37.unknown1, SERVER_UNKNOWN_37_UNKNOWN1);
				bn_int_set(&rpacket->u.server_unknown_37.unknown2, SERVER_UNKNOWN_37_UNKNOWN2);

				if (!(charlist = account_get_closed_characterlist(conn_get_account(c), conn_get_clienttag(c), realm_get_name(conn_get_realm(c))))) {
					bn_int_set(&rpacket->u.server_unknown_37.count, 0);
					if (pvpgn_v3_send_charlistreply(c,
					        SERVER_UNKNOWN_37_UNKNOWN1,
					        SERVER_UNKNOWN_37_UNKNOWN2,
					        0u,
					        nullptr,
					        0u) == 1) {
						packet_del_ref(rpacket);
						return 0;
					}
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
					return 0;
				}
				temp = ([&](){ std::size_t n = std::strlen(charlist) + 1; char* r = new char[n]; std::memcpy(r, charlist, n); return r; })();

				{
					char const *tok1;
					char const *tok2;
					t_character const *ch;
					unsigned int count;

					count = 0;
					tok1 = (char const *)std::strtok(temp, ",");	/* std::strtok modifies the string it is passed */
					tok2 = std::strtok(NULL, ",");
					while (tok1) {
						if (!tok2) {
							eventlog(eventlog_level_error, __FUNCTION__, "[{}] account \"{}\" has bad character list \"{}\"", conn_get_socket(c), conn_get_username(c), temp);
							break;
						}

						if ((ch = characterlist_find_character(tok1, tok2))) {
							packet_append_ntstring(rpacket, character_get_realmname(ch));
							packet_append_ntstring(rpacket, ",");
							packet_append_string(rpacket, character_get_name(ch));
							packet_append_string(rpacket, character_get_playerinfo(ch));
							packet_append_string(rpacket, character_get_guildname(ch));
							count++;
						}
						else
							eventlog(eventlog_level_error, __FUNCTION__, "[{}] character \"{}\" is missing", conn_get_socket(c), tok2);
						tok1 = std::strtok(NULL, ",");
						tok2 = std::strtok(NULL, ",");
					}
					delete[] temp;

					bn_int_set(&rpacket->u.server_unknown_37.count, count);
					{
						// Extract char_data bytes from the freshly-built rpacket
						// (everything after the fixed header).
						std::size_t v3_hdr = sizeof(t_server_unknown_37);
						std::size_t v3_end = packet_get_size(rpacket);
						unsigned char const* v3_data = nullptr;
						unsigned int v3_data_len = 0u;
						if (v3_end > v3_hdr) {
							v3_data = reinterpret_cast<unsigned char const*>(
							    packet_get_data_const(rpacket, v3_hdr, 1));
							v3_data_len = static_cast<unsigned int>(v3_end - v3_hdr);
						}
						if (pvpgn_v3_send_charlistreply(c,
						        SERVER_UNKNOWN_37_UNKNOWN1,
						        SERVER_UNKNOWN_37_UNKNOWN2,
						        count,
						        v3_data,
						        v3_data_len) == 1) {
							packet_del_ref(rpacket);
							return 0;
						}
					}
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
				}
			}

			return 0;
		}

		int _client_unknown39(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_stub_dispatch(c, "unknown39");
			if (packet_get_size(packet) < sizeof(t_client_unknown_39)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad UNKNOWN_39 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_unknown_39), packet_get_size(packet));
				return -1;
			}
			return 0;
		}


}} // namespace pvpgn::bnetd
