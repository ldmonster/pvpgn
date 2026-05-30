// =====================================================================
// auth_version.cpp — Version check, auth request, icon, echo, ping handlers
// Split from auth.cpp (plan 15 §3 / SOLID-S refactor)
// Handles: authreq1 (SC/BW/D2 version check), authreq109 (W3 version check),
//          regsnoopreply, iconreq, echoreply, pingreq
// =====================================================================
#include "handle_bnet_internal.h"

namespace pvpgn { namespace bnetd {

		int _client_echoreply(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_keepalive_dispatch(c, "echoreply");
			if (packet_get_size(packet) < sizeof(t_client_echoreply)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad ECHOREPLY packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_echoreply), packet_get_size(packet));
				return -1;
			}

			{
				unsigned int now;
				unsigned int then;

				now = get_ticks();
				then = bn_int_get(packet->u.client_echoreply.ticks);
				if (!now || !then || now < then)
					eventlog(eventlog_level_warn, __FUNCTION__, "[{}] bad timing in echo reply: now={} then={}", conn_get_socket(c), now, then);
				else
					conn_set_latency(c, now - then);
			}

			return 0;
		}

		int _client_authreq1(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_auth_dispatch(c, "authreq1");
			eventlog(eventlog_level_trace, __FUNCTION__, "[{}] received AUTHREQ1(0x07) packet", conn_get_socket(c));

			if (packet_get_size(packet) < sizeof(t_client_authreq1))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad AUTHREQ1 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_authreq1), packet_get_size(packet));
				return -1;
			}

			auto send_failed_packet = [](t_connection *c)
			{
				conn_set_state(c, conn_state_untrusted);
				// v3 strangler-fig: build BADVERSION reply via the
				// codec and ship through the registered handler.
				if (pvpgn_v3_send_authreply1(
				        c, SERVER_AUTHREPLY1_MESSAGE_BADVERSION,
				        nullptr) == 1)
				{
					return;
				}
				t_packet *rpacket = packet_create(packet_class_bnet);
				if (rpacket)
				{
					packet_set_size(rpacket, sizeof(t_server_authreply1));
					packet_set_type(rpacket, SERVER_AUTHREPLY1);

					bn_int_set(&rpacket->u.server_authreply1.message, SERVER_AUTHREPLY1_MESSAGE_BADVERSION);
					packet_append_string(rpacket, "");
					packet_append_string(rpacket, ""); // undocumented extra null terminator

					conn_push_outqueue(c, rpacket);
					
					packet_del_ref(rpacket);
				}
			};

			// The following if statements are sanity checks
			// The client should have already sent this information in a previous packet and is resending it again in this packet
			if (bn_int_get(packet->u.client_authreq1.archtag) != conn_get_archtag(c))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad AUTHREQ1 (mismatch architecture)", conn_get_socket(c));
				send_failed_packet(c);
				return -1;
			}

			if (bn_int_get(packet->u.client_authreq1.clienttag) != conn_get_clienttag(c))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad AUTHREQ1 (mismatch client tag)", conn_get_socket(c));
				send_failed_packet(c);
				return -1;
			}

			if (bn_int_get(packet->u.client_authreq1.versionid) != conn_get_versionid(c))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad AUTHREQ1 (mismatch version ID)", conn_get_socket(c));
				send_failed_packet(c);
				return -1;
			}


			const char *exeinfo = packet_get_str_const(packet, sizeof(t_client_authreq1), MAX_EXEINFO_STR);
			if (exeinfo)
			{
				conn_set_clientexe(c, exeinfo);
			}
			else
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad AUTHREQ1 (missing or too long exeinfo)", conn_get_socket(c));
				send_failed_packet(c);
				return 0;
			}

			std::string version_string = vernum_to_verstr(bn_int_get(packet->u.client_authreq1.gameversion));
			conn_set_clientver(c, version_string.c_str());
			eventlog(eventlog_level_info, __FUNCTION__, "[{}] CLIENT_AUTHREQ1 archtag=0x{:08x} clienttag=0x{:08x} verstr={} exeinfo=\"{}\" versionid=0x{:08x} gameversion=0x{:08x} checksum=0x{:08x}", conn_get_socket(c), bn_int_get(packet->u.client_authreq1.archtag), bn_int_get(packet->u.client_authreq1.clienttag), version_string, exeinfo, conn_get_versionid(c), conn_get_gameversion(c), conn_get_checksum(c));

			conn_set_versionid(c, bn_int_get(packet->u.client_authreq1.versionid));
			conn_set_checksum(c, bn_int_get(packet->u.client_authreq1.checksum));
			conn_set_gameversion(c, bn_int_get(packet->u.client_authreq1.gameversion));

			
			const VersionCheck* vc = select_versioncheck(conn_get_archtag(c), conn_get_clienttag(c), conn_get_versionid(c), conn_get_gameversion(c), conn_get_checksum(c));
			conn_set_versioncheck(c, vc);
			if (vc)
			{
				eventlog(eventlog_level_info, __FUNCTION__, "[{}] client matches versiontag \"{}\"", conn_get_socket(c), conn_get_versioncheck(c)->get_version_tag());
			}
			else
			{
				if (prefs_v3::allow_unknown_version())
				{
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] skipping versioncheck because allow_unknown_version is true", conn_get_socket(c));
				}
				else
				{
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] client failed versioncheck", conn_get_socket(c));
					send_failed_packet(c);
					return 0;
				}
			}


			t_packet *rpacket = packet_create(packet_class_bnet);
			if (rpacket)
			{
				packet_set_size(rpacket, sizeof(t_server_authreply1));
				packet_set_type(rpacket, SERVER_AUTHREPLY1);

				char *mpqfilename = nullptr;
				if (vc)
				{
					mpqfilename = autoupdate_check(conn_get_archtag(c), conn_get_clienttag(c), conn_get_gamelang(c), vc->get_version_tag().c_str(), nullptr);
				}

				// Only handle updates when there is an update file available.
				if (mpqfilename)
				{
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] an upgrade for version {} is available \"{}\"", conn_get_socket(c), conn_get_versioncheck(c)->get_version_tag(), mpqfilename);
					bn_int_set(&rpacket->u.server_authreply1.message, SERVER_AUTHREPLY1_MESSAGE_UPDATE);
					packet_append_string(rpacket, mpqfilename);
				}
				else
				{
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] no upgrade is available", conn_get_socket(c));
				}

				bn_int_set(&rpacket->u.server_authreply1.message, SERVER_AUTHREPLY1_MESSAGE_OK);
				packet_append_string(rpacket, "");
				packet_append_string(rpacket, ""); // FIXME: what's the second string for?

				// v3 strangler-fig: emit the reply via the v3 codec
				// instead of the legacy packet, when a send_packet
				// handler is installed. Mirrors the legacy quirk
				// that the message code is OK even when a mpq
				// filename is present (filename is prepended; two
				// trailing empties are always appended).
				if (pvpgn_v3_send_authreply1(
				        c, SERVER_AUTHREPLY1_MESSAGE_OK,
				        mpqfilename) == 1)
				{
					packet_del_ref(rpacket);
					if (mpqfilename)
						delete[] mpqfilename;
					return 0;
				}

				if (mpqfilename)
					delete[] mpqfilename;

				conn_push_outqueue(c, rpacket);

				packet_del_ref(rpacket);
			}

			return 0;
		}

		int _client_authreq109(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_auth_dispatch(c, "authreq109");
			if (packet_get_size(packet) < sizeof(t_client_authreq_109))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad AUTHREQ_109 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_authreq_109), packet_get_size(packet));
				return 0;
			}

			auto send_failed_packet = [](t_connection *c)
			{
				conn_set_state(c, conn_state_untrusted);
				if (pvpgn_v3_send_authreply109(
				        c, SERVER_AUTHREPLY_109_MESSAGE_BADVERSION,
				        nullptr) == 1)
				{
					return;
				}
				t_packet *rpacket = packet_create(packet_class_bnet);
				if (rpacket)
				{
					packet_set_size(rpacket, sizeof(t_server_authreply_109));
					packet_set_type(rpacket, SERVER_AUTHREPLY_109);

					bn_int_set(&rpacket->u.server_authreply_109.message, SERVER_AUTHREPLY_109_MESSAGE_BADVERSION);
					packet_append_string(rpacket, "");

					conn_push_outqueue(c, rpacket);

					packet_del_ref(rpacket);
				}
			};


			std::uint32_t count = bn_int_get(packet->u.client_authreq_109.cdkey_number);
			std::size_t position = sizeof(t_client_authreq_109) + (count * sizeof(t_cdkey_info));

			const char *const exeinfo = packet_get_str_const(packet, position, MAX_EXEINFO_STR);
			if (exeinfo)
			{
				conn_set_clientexe(c, exeinfo);
			}
			else
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad AUTHREQ_109 (missing or too long exeinfo)", conn_get_socket(c));
				send_failed_packet(c);
				return 0;
			}

			position += std::strlen(exeinfo) + 1;

			const char *const owner = packet_get_str_const(packet, position, MAX_OWNER_STR);
			if (owner)
			{
				conn_set_owner(c, owner);
			}
			else
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad AUTHREQ_109 (missing or too long owner)", conn_get_socket(c));
				conn_set_owner(c, "");
			}

			conn_set_checksum(c, bn_int_get(packet->u.client_authreq_109.checksum));
			conn_set_gameversion(c, bn_int_get(packet->u.client_authreq_109.gameversion));
			std::string version = vernum_to_verstr(bn_int_get(packet->u.client_authreq_109.gameversion));
			conn_set_clientver(c, version.c_str());

			eventlog(eventlog_level_info, __FUNCTION__, "[{}] CLIENT_AUTHREQ_109 ticks=0x{:08x}, verstr={} exeinfo=\"{}\" versionid=0x{:08x} gameversion=0x{:08x} checksum=0x{:08x}", conn_get_socket(c), bn_int_get(packet->u.client_authreq_109.ticks), version, exeinfo, conn_get_versionid(c), conn_get_gameversion(c), conn_get_checksum(c));

			t_packet *rpacket;
			if ((rpacket = packet_create(packet_class_bnet)))
			{
				packet_set_size(rpacket, sizeof(t_server_authreply_109));
				packet_set_type(rpacket, SERVER_AUTHREPLY_109);


				const VersionCheck* vc = select_versioncheck(conn_get_archtag(c), conn_get_clienttag(c), conn_get_versionid(c), conn_get_gameversion(c), conn_get_checksum(c));
				conn_set_versioncheck(c, vc);
				if (vc)
				{
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] client matches versiontag \"{}\"", conn_get_socket(c), conn_get_versioncheck(c)->get_version_tag());
				}
				else
				{
					if (prefs_v3::allow_unknown_version())
					{
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] skipping versioncheck because allow_unknown_version is true", conn_get_socket(c));
					}
					else
					{
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] client failed versioncheck", conn_get_socket(c));
						packet_del_ref(rpacket);
						send_failed_packet(c);
						return 0;
					}
				}


				char *mpqfilename = nullptr;
				if (vc)
				{
					mpqfilename = autoupdate_check(conn_get_archtag(c), conn_get_clienttag(c), conn_get_gamelang(c), conn_get_versioncheck(c)->get_version_tag().c_str(), NULL);
				}

				// Only handle updates when there is an update file available.
				if (mpqfilename)
				{
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] an upgrade for {} is available \"{}\"", conn_get_socket(c), conn_get_versioncheck(c)->get_version_tag(), mpqfilename);
					bn_int_set(&rpacket->u.server_authreply_109.message, SERVER_AUTHREPLY_109_MESSAGE_UPDATE);
					packet_append_string(rpacket, mpqfilename);
				}
				else
				{
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] no upgrade is available", conn_get_socket(c));
				}

				bn_int_set(&rpacket->u.server_authreply_109.message, SERVER_AUTHREPLY_109_MESSAGE_OK);
				packet_append_string(rpacket, "");

				// v3 strangler-fig: emit via the v3 codec when the
				// send_packet handler is installed. Mirrors the
				// legacy quirk that the final message code is OK
				// regardless of whether an update filename was
				// prepended.
				if (pvpgn_v3_send_authreply109(
				        c, SERVER_AUTHREPLY_109_MESSAGE_OK,
				        mpqfilename) == 1)
				{
					if (mpqfilename)
						delete[] mpqfilename;
					packet_del_ref(rpacket);
					return 0;
				}

				if (mpqfilename)
					delete[] mpqfilename;

				conn_push_outqueue(c, rpacket);

				packet_del_ref(rpacket);
			}

			return 0;
		}

		int _client_regsnoopreply(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_stub_dispatch(c, "regsnoopreply");
			if (packet_get_size(packet) < sizeof(t_client_regsnoopreply)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad REGSNOOPREPLY packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_regsnoopreply), packet_get_size(packet));
				return -1;
			}
			return 0;
		}

		int _client_iconreq(t_connection * c, t_packet const *const packet)
		{
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_iconreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad ICONREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_iconreq), packet_get_size(packet));
				return -1;
			}

			if ((rpacket = packet_create(packet_class_bnet))) {
				packet_set_size(rpacket, sizeof(t_server_iconreply));
				packet_set_type(rpacket, SERVER_ICONREPLY);
				file_to_mod_time(c, prefs_v3::iconfile(), &rpacket->u.server_iconreply.timestamp);

				/* battle.net sends different file on iconreq for WAR3 and W3XP [Omega] */
				if ((conn_get_clienttag(c) == CLIENTTAG_WARCRAFT3_UINT) || (conn_get_clienttag(c) == CLIENTTAG_WAR3XP_UINT))
					packet_append_string(rpacket, prefs_v3::war3_iconfile());
				/* battle.net still sends "icons.bni" to sc/bw clients
				 * clients request icons_STAR.bni seperatly */
				/*	else if (std::strcmp(conn_get_clienttag(c),CLIENTTAG_STARCRAFT)==0)
						packet_append_string(rpacket,prefs_v3::star_iconfile());
						else if (std::strcmp(conn_get_clienttag(c),CLIENTTAG_BROODWARS)==0)
						packet_append_string(rpacket,prefs_v3::star_iconfile());
						*/
				else
					packet_append_string(rpacket, prefs_v3::iconfile());

				{
					// Pull timestamp + appended filename back out of the
					// legacy-built rpacket so all branch logic stays intact.
					unsigned long long v3_ts = bn_long_get(
						rpacket->u.server_iconreply.timestamp);
					char const* v3_filename = nullptr;
					if (packet_get_size(rpacket)
					    > sizeof(t_server_iconreply)) {
						v3_filename = reinterpret_cast<char const*>(
							packet_get_data_const(
								rpacket,
								sizeof(t_server_iconreply),
								1));
					}
					if (pvpgn_v3_send_iconreply(c, v3_ts, v3_filename) == 1) {
						packet_del_ref(rpacket);
						return 0;
					}
				}
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}

			return 0;
		}

		int _client_pingreq(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_keepalive_dispatch(c, "pingreq");
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_pingreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad PINGREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_pingreq), packet_get_size(packet));
				return -1;
			}

				if (pvpgn_v3_send_pingreply(c) <= 0)
				if ((rpacket = packet_create(packet_class_bnet))) {
					packet_set_size(rpacket, sizeof(t_server_pingreply));
					packet_set_type(rpacket, SERVER_PINGREPLY);
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
				}

			return 0;
		}

}} // namespace pvpgn::bnetd
