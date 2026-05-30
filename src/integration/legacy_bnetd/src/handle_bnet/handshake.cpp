// =====================================================================
// Auto-split from handle_bnet_link.cpp by scripts/dev/split_handle_bnet.py
// See plans/15-large-file-decomposition-detail.md for rationale.
// =====================================================================
#include "handle_bnet_internal.h"

namespace pvpgn { namespace bnetd {

		int _client_unknown_1b(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_stub_dispatch(c, "unknown_1b");
			if (packet_get_size(packet) < sizeof(t_client_unknown_1b)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad UNKNOWN_1B packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_unknown_1b), packet_get_size(packet));
				return -1;
			}

			{
				unsigned int newip;
				unsigned short newport;

				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] UNKNOWN_1B unknown1=0x{:04hx}", conn_get_socket(c), bn_short_get(packet->u.client_unknown_1b.unknown1));
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] UNKNOWN_1B unknown2=0x{:08x}", conn_get_socket(c), bn_int_get(packet->u.client_unknown_1b.unknown2));
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] UNKNOWN_1B unknown3=0x{:08x}", conn_get_socket(c), bn_int_get(packet->u.client_unknown_1b.unknown3));

				newip = bn_int_nget(packet->u.client_unknown_1b.ip);
				newport = bn_short_nget(packet->u.client_unknown_1b.port);

				eventlog(eventlog_level_info, __FUNCTION__, "[{}] UNKNOWN_1B set new UDP address to {}", conn_get_socket(c), addr_num_to_addr_str(newip, newport));
				conn_set_game_addr(c, newip);
				conn_set_game_port(c, newport);
			}
			return 0;
		}

		int _client_compinfo1(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_handshake_dispatch(c, "compinfo1");
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_compinfo1)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad COMPINFO1 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_compinfo1), packet_get_size(packet));
				return -1;
			}

			{
				char const *host;
				char const *user;

				if (!(host = packet_get_str_const(packet, sizeof(t_client_compinfo1), MAX_WINHOST_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad COMPINFO1 packet (missing or too long host)", conn_get_socket(c));
					return -1;
				}
				if (!(user = packet_get_str_const(packet, sizeof(t_client_compinfo1)+std::strlen(host) + 1, MAX_WINUSER_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad COMPINFO1 packet (missing or too long user)", conn_get_socket(c));
					return -1;
				}

				conn_set_host(c, host);
				conn_set_user(c, user);
			}

				if (pvpgn_v3_send_compreply(c) <= 0)
				if ((rpacket = packet_create(packet_class_bnet))) {
					packet_set_size(rpacket, sizeof(t_server_compreply));
					packet_set_type(rpacket, SERVER_COMPREPLY);
					bn_int_set(&rpacket->u.server_compreply.reg_version, SERVER_COMPREPLY_REG_VERSION);
					bn_int_set(&rpacket->u.server_compreply.reg_auth, SERVER_COMPREPLY_REG_AUTH);
					bn_int_set(&rpacket->u.server_compreply.client_id, SERVER_COMPREPLY_CLIENT_ID);
					bn_int_set(&rpacket->u.server_compreply.client_token, SERVER_COMPREPLY_CLIENT_TOKEN);
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
				}
				if (pvpgn_v3_send_sessionkey1(c, static_cast<unsigned int>(conn_get_sessionkey(c))) <= 0)
				if ((rpacket = packet_create(packet_class_bnet))) {
					packet_set_size(rpacket, sizeof(t_server_sessionkey1));
					packet_set_type(rpacket, SERVER_SESSIONKEY1);
					bn_int_set(&rpacket->u.server_sessionkey1.sessionkey, conn_get_sessionkey(c));
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
				}
				return 0;
			}

		int _client_compinfo2(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_handshake_dispatch(c, "compinfo2");
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_compinfo2)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad COMPINFO2 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_compinfo2), packet_get_size(packet));
				return -1;
			}

			{
				char const *host;
				char const *user;

				if (!(host = packet_get_str_const(packet, sizeof(t_client_compinfo2), MAX_WINHOST_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad COMPINFO2 packet (missing or too long host)", conn_get_socket(c));
					return -1;
				}
				if (!(user = packet_get_str_const(packet, sizeof(t_client_compinfo2)+std::strlen(host) + 1, MAX_WINUSER_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad COMPINFO2 packet (missing or too long user)", conn_get_socket(c));
					return -1;
				}

				conn_set_host(c, host);
				conn_set_user(c, user);
			}

				if (pvpgn_v3_send_compreply(c) <= 0)
				if ((rpacket = packet_create(packet_class_bnet))) {
					packet_set_size(rpacket, sizeof(t_server_compreply));
					packet_set_type(rpacket, SERVER_COMPREPLY);
					bn_int_set(&rpacket->u.server_compreply.reg_version, SERVER_COMPREPLY_REG_VERSION);
					bn_int_set(&rpacket->u.server_compreply.reg_auth, SERVER_COMPREPLY_REG_AUTH);
					bn_int_set(&rpacket->u.server_compreply.client_id, SERVER_COMPREPLY_CLIENT_ID);
					bn_int_set(&rpacket->u.server_compreply.client_token, SERVER_COMPREPLY_CLIENT_TOKEN);
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
				}
	
				if (pvpgn_v3_send_sessionkey2(c,
				                              static_cast<unsigned int>(conn_get_sessionnum(c)),
				                              static_cast<unsigned int>(conn_get_sessionkey(c))) <= 0)
				if ((rpacket = packet_create(packet_class_bnet))) {
					packet_set_size(rpacket, sizeof(t_server_sessionkey2));
					packet_set_type(rpacket, SERVER_SESSIONKEY2);
					bn_int_set(&rpacket->u.server_sessionkey2.sessionnum, conn_get_sessionnum(c));
					bn_int_set(&rpacket->u.server_sessionkey2.sessionkey, conn_get_sessionkey(c));
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
				}
	
				return 0;
			}

		int _client_countryinfo1(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_handshake_dispatch(c, "countryinfo1");
			if (packet_get_size(packet) < sizeof(t_client_countryinfo1)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad COUNTRYINFO1 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_countryinfo1), packet_get_size(packet));
				return -1;
			}
			{
				char const *langstr;
				char const *countrycode;
				char const *country;
				unsigned int tzbias;

				if (!(langstr = packet_get_str_const(packet, sizeof(t_client_countryinfo1), MAX_LANG_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad COUNTRYINFO1 packet (missing or too long langstr)", conn_get_socket(c));
					return -1;
				}

				if (!(countrycode = packet_get_str_const(packet, sizeof(t_client_countryinfo1)+std::strlen(langstr) + 1, MAX_COUNTRYCODE_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad COUNTRYINFO1 packet (missing or too long countrycode)", conn_get_socket(c));
					return -1;
				}

				if (!(country = packet_get_str_const(packet, sizeof(t_client_countryinfo1)+std::strlen(langstr) + 1 + std::strlen(countrycode) + 1, MAX_COUNTRY_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad COUNTRYINFO1 packet (missing or too long country)", conn_get_socket(c));
					return -1;
				}

				if (!(packet_get_str_const(packet, sizeof(t_client_countryinfo1)+std::strlen(langstr) + 1 + std::strlen(countrycode) + 1 + std::strlen(country) + 1, MAX_COUNTRYNAME_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad COUNTRYINFO1 packet (missing or too long countryname)", conn_get_socket(c));
					return -1;
				}

				tzbias = bn_int_get(packet->u.client_countryinfo1.bias);
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] COUNTRYINFO1 packet from tzbias=0x{:04x} langstr={} countrycode={} country={}", conn_get_socket(c), tzbias, langstr, countrycode, country);
				conn_set_country(c, country);
				conn_set_tzbias(c, uint32_to_int(tzbias));
			}
			return 0;
		}

		int _client_auth_info(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_auth_dispatch(c, "auth_info");
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_auth_info)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad AUTH_INFO packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_auth_info), packet_get_size(packet));
				return -1;
			}

			{
				char const *langstr;
				char const *countryname;
				unsigned int tzbias;
				char archtag_str[5];
				char clienttag_str[5];
				char gamelang_str[5];

				if (!(langstr = packet_get_str_const(packet, sizeof(t_client_auth_info), MAX_LANG_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad AUTH_INFO packet (missing or too long langstr)", conn_get_socket(c));
					return -1;
				}

				if (!(countryname = packet_get_str_const(packet, sizeof(t_client_auth_info)+std::strlen(langstr) + 1, MAX_COUNTRYNAME_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad AUTH_INFO packet (missing or too long countryname)", conn_get_socket(c));
					return -1;
				}

				/* check if it's an allowed client type */
				if (tag_check_in_list(bn_int_get(packet->u.client_auth_info.clienttag), prefs_v3::allowed_clients())) {
					conn_set_state(c, conn_state_destroy);
					return 0;
				}
				
				tzbias = bn_int_get(packet->u.client_auth_info.bias);

				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] AUTH_INFO packet {{ protocol={:#02x}, platform={}, product={}, versionid={:#02x}, language={}, localip={:#04x}, tzbias={:04x}, locale={}, language={}, country={}.{} }}",
					conn_get_socket(c),
					bn_int_get(packet->u.client_auth_info.protocol),
					tag_uint_to_str(archtag_str, bn_int_get(packet->u.client_auth_info.archtag)),
					tag_uint_to_str(clienttag_str, bn_int_get(packet->u.client_auth_info.clienttag)),
					bn_int_get(packet->u.client_auth_info.versionid),
					tag_uint_to_str(gamelang_str, bn_int_get(packet->u.client_auth_info.gamelang)),
					bn_int_get(packet->u.client_auth_info.localip),
					tzbias,
					bn_int_get(packet->u.client_auth_info.lcid),
					bn_int_get(packet->u.client_auth_info.langid),
					langstr,
					countryname);

				conn_set_country(c, langstr);	/* FIXME: This isn't right.  We want USA not ENU (English-US) */
				conn_set_tzbias(c, uint32_to_int(tzbias));
				conn_set_versionid(c, bn_int_get(packet->u.client_auth_info.versionid));
				conn_set_archtag(c, bn_int_get(packet->u.client_auth_info.archtag));
				conn_set_clienttag(c, bn_int_get(packet->u.client_auth_info.clienttag));
				conn_set_gamelang(c, bn_int_get(packet->u.client_auth_info.gamelang));

				/* First, send an ECHO_REQ */

				if (pvpgn_v3_send_echoreq(c, static_cast<unsigned int>(get_ticks())) <= 0)
				if ((rpacket = packet_create(packet_class_bnet))) {
					packet_set_size(rpacket, sizeof(t_server_echoreq));
					packet_set_type(rpacket, SERVER_ECHOREQ);
					bn_int_set(&rpacket->u.server_echoreq.ticks, get_ticks());
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
				}

				if ((rpacket = packet_create(packet_class_bnet)))
				{
					packet_set_size(rpacket, sizeof(t_server_authreq_109));
					packet_set_type(rpacket, SERVER_AUTHREQ_109);

					// Logon type
					if ((conn_get_clienttag(c) == CLIENTTAG_WARCRAFT3_UINT))
						bn_int_set(&rpacket->u.server_authreq_109.logontype, SERVER_AUTHREQ_109_LOGONTYPE_W3);
					else if ((conn_get_clienttag(c) == CLIENTTAG_WAR3XP_UINT))
						bn_int_set(&rpacket->u.server_authreq_109.logontype, SERVER_AUTHREQ_109_LOGONTYPE_W3XP);
					else
						bn_int_set(&rpacket->u.server_authreq_109.logontype, SERVER_AUTHREQ_109_LOGONTYPE);


					// Session
					bn_int_set(&rpacket->u.server_authreq_109.sessionkey, conn_get_sessionkey(c));
					bn_int_set(&rpacket->u.server_authreq_109.sessionnum, conn_get_sessionnum(c));


					// CheckRevision
					std::tuple<std::string, std::string> checkrevision = select_checkrevision(bn_int_get(packet->u.client_auth_info.archtag), bn_int_get(packet->u.client_auth_info.clienttag), bn_int_get(packet->u.client_auth_info.versionid));
					file_to_mod_time(c, std::get<0>(checkrevision).c_str(), &rpacket->u.server_authreq_109.timestamp); // Checkrevision file timestamp
					packet_append_string(rpacket, std::get<0>(checkrevision).c_str()); // CheckRevision filename
					packet_append_string(rpacket, std::get<1>(checkrevision).c_str()); // CheckRevision equation
					eventlog(eventlog_level_debug, __FUNCTION__, "[{}] selected \"{}\" \"{}\"", conn_get_socket(c), std::get<0>(checkrevision), std::get<1>(checkrevision));
					

					// WarCraft 3 Server Signature
					if ((conn_get_clienttag(c) == CLIENTTAG_WARCRAFT3_UINT)
						|| (conn_get_clienttag(c) == CLIENTTAG_WAR3XP_UINT))
					{
						char padding[128];
						std::memset(padding, 0, 128);
						packet_append_data(rpacket, padding, 128);
					}

					// Strangler-fig: build the same bytes via the v3
					// codec and dispatch through pvpgn_v3_send_packet.
					// On success skip the legacy outqueue push (we
					// still keep the legacy rpacket build above so any
					// side effects -- e.g. file_to_mod_time, eventlog
					// -- match the legacy path exactly).
					{
						const unsigned int v3_logontype =
							(conn_get_clienttag(c) == CLIENTTAG_WARCRAFT3_UINT)
								? SERVER_AUTHREQ_109_LOGONTYPE_W3
								: (conn_get_clienttag(c) == CLIENTTAG_WAR3XP_UINT)
									? SERVER_AUTHREQ_109_LOGONTYPE_W3XP
									: SERVER_AUTHREQ_109_LOGONTYPE;
						const int v3_w3sig =
							((conn_get_clienttag(c) == CLIENTTAG_WARCRAFT3_UINT) ||
							 (conn_get_clienttag(c) == CLIENTTAG_WAR3XP_UINT))
								? 1 : 0;
						const unsigned long long v3_timestamp =
							bn_long_get(rpacket->u.server_authreq_109.timestamp);
						int v3rc = pvpgn_v3_send_authinfo_reply(
							c,
							v3_logontype,
							conn_get_sessionkey(c),
							conn_get_sessionnum(c),
							v3_timestamp,
							std::get<0>(checkrevision).c_str(),
							std::get<1>(checkrevision).c_str(),
							v3_w3sig);
						if (v3rc == 1) {
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

		int _client_unknown2b(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_stub_dispatch(c, "unknown2b");
			if (packet_get_size(packet) < sizeof(t_client_unknown_2b)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad UNKNOWN_2B packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_unknown_2b), packet_get_size(packet));
				return -1;
			}
			return 0;
		}

		int _client_progident(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_progident_dispatch(c, "progident");
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_progident)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad PROGIDENT packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_progident), packet_get_size(packet));
				return -1;
			}

			if (tag_check_in_list(bn_int_get(packet->u.client_progident.clienttag), prefs_v3::allowed_clients())) {
				conn_set_state(c, conn_state_destroy);
				return 0;
			}

			eventlog(eventlog_level_debug, __FUNCTION__, "[{}] CLIENT_PROGIDENT archtag=0x{:08x} clienttag=0x{:08x} versionid=0x{:08x} unknown1=0x{:08x}", conn_get_socket(c), bn_int_get(packet->u.client_progident.archtag), bn_int_get(packet->u.client_progident.clienttag), bn_int_get(packet->u.client_progident.versionid), bn_int_get(packet->u.client_progident.unknown1));

			conn_set_archtag(c, bn_int_get(packet->u.client_progident.archtag));
			conn_set_clienttag(c, bn_int_get(packet->u.client_progident.clienttag));
			conn_set_versionid(c, bn_int_get(packet->u.client_progident.versionid));

			{
				// CheckRevision challenge — compute filename + equation first so
				// both the v3 bridge and the legacy path can use them.
				std::tuple<std::string, std::string> checkrevision = select_checkrevision(bn_int_get(packet->u.client_progident.archtag), bn_int_get(packet->u.client_progident.clienttag), bn_int_get(packet->u.client_progident.versionid));
				eventlog(eventlog_level_debug, __FUNCTION__, "[{}] selected \"{}\" \"{}\"", conn_get_socket(c), std::get<0>(checkrevision), std::get<1>(checkrevision));

				// Obtain the file modification timestamp (needed by both paths).
				bn_long ts_bn{};
				file_to_mod_time(c, std::get<0>(checkrevision).c_str(), &ts_bn);

				// v3 strangler-fig: build SERVER_AUTHREQ1 via the codec and
				// ship through the registered send_packet handler.
				if (pvpgn_v3_send_authreq1_server(
				        c,
				        static_cast<unsigned long long>(bn_long_get(ts_bn)),
				        std::get<0>(checkrevision).c_str(),
				        std::get<1>(checkrevision).c_str()) == 1)
				{
					// Bridge succeeded — skip legacy emit path.
					goto _client_progident_done;
				}

				if ((rpacket = packet_create(packet_class_bnet)))
				{
					packet_set_size(rpacket, sizeof(t_server_authreq1));
					packet_set_type(rpacket, SERVER_AUTHREQ1);

					bn_long_set(&rpacket->u.server_authreq1.timestamp, bn_long_get(ts_bn)); // Checkrevision file timestamp
					packet_append_string(rpacket, std::get<0>(checkrevision).c_str()); // CheckRevision filename
					packet_append_string(rpacket, std::get<1>(checkrevision).c_str()); // CheckRevision equation

					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
				}
			}
			_client_progident_done:

			return 0;
		}


}} // namespace pvpgn::bnetd
