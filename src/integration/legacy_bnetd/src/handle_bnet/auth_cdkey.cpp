// =====================================================================
// auth_cdkey.cpp — CD-key validation handlers
// Split from auth.cpp (plan 15 §3 / SOLID-S refactor)
// Handles: cdkey (SC/BW), cdkey2 (D2), cdkey3 (W3)
// =====================================================================
#include "handle_bnet_internal.h"

namespace pvpgn { namespace bnetd {

		int _client_cdkey(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_cdkey_dispatch(c, "cdkey");
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_cdkey)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CDKEY packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_cdkey), packet_get_size(packet));
				return -1;
			}

			{
				char const *cdkey;
				char const *owner;

				if (!(cdkey = packet_get_str_const(packet, sizeof(t_client_cdkey), MAX_CDKEY_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CDKEY packet (missing or too long cdkey)", conn_get_socket(c));
					return -1;
				}
				if (!(owner = packet_get_str_const(packet, sizeof(t_client_cdkey)+std::strlen(cdkey) + 1, MAX_OWNER_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CDKEY packet (missing or too long owner)", conn_get_socket(c));
					return -1;
				}

				conn_set_cdkey(c, cdkey);
				conn_set_owner(c, owner);

				if (pvpgn_v3_send_cdkeyreply(c,
				                             SERVER_CDKEYREPLY_MESSAGE_OK,
				                             owner) <= 0)
				if ((rpacket = packet_create(packet_class_bnet))) {
					packet_set_size(rpacket, sizeof(t_server_cdkeyreply));
					packet_set_type(rpacket, SERVER_CDKEYREPLY);
					bn_int_set(&rpacket->u.server_cdkeyreply.message, SERVER_CDKEYREPLY_MESSAGE_OK);
					packet_append_string(rpacket, owner);
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
				}
			}
#if 0				/* Blizzard used this to track down pirates, should only be accepted by old clients */
			if ((rpacket = packet_create(packet_class_bnet))) {
				packet_set_size(rpacket, sizeof(t_server_regsnoopreq));
				packet_set_type(rpacket, SERVER_REGSNOOPREQ);
				bn_int_set(&rpacket->u.server_regsnoopreq.unknown1, SERVER_REGSNOOPREQ_UNKNOWN1);	/* sequence num */
				bn_int_set(&rpacket->u.server_regsnoopreq.hkey, SERVER_REGSNOOPREQ_HKEY_CURRENT_USER);
				packet_append_string(rpacket, SERVER_REGSNOOPREQ_REGKEY);
				packet_append_string(rpacket, SERVER_REGSNOOPREQ_REGVALNAME);
				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}
#endif
			return 0;
		}

		int _client_cdkey2(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_cdkey_dispatch(c, "cdkey2");
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_cdkey2)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CDKEY2 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_cdkey2), packet_get_size(packet));
				return -1;
			}

			{
				char const *owner;

				if (!(owner = packet_get_str_const(packet, sizeof(t_client_cdkey2), MAX_OWNER_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CDKEY2 packet (missing or too long owner)", conn_get_socket(c));
					return -1;
				}

				conn_set_owner(c, owner);

				if (pvpgn_v3_send_cdkeyreply2(c,
				                              SERVER_CDKEYREPLY2_MESSAGE_OK,
				                              owner) <= 0)
				if ((rpacket = packet_create(packet_class_bnet))) {
					packet_set_size(rpacket, sizeof(t_server_cdkeyreply2));
					packet_set_type(rpacket, SERVER_CDKEYREPLY2);
					bn_int_set(&rpacket->u.server_cdkeyreply2.message, SERVER_CDKEYREPLY2_MESSAGE_OK);
					packet_append_string(rpacket, owner);
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
				}
			}

			return 0;
		}

		int _client_cdkey3(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_cdkey_dispatch(c, "cdkey3");
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_cdkey3)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CDKEY3 packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_cdkey2), packet_get_size(packet));
				return -1;
			}

			{
				char const *owner;

				if (!(owner = packet_get_str_const(packet, sizeof(t_client_cdkey3), MAX_OWNER_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad CDKEY3 packet (missing or too long owner)", conn_get_socket(c));
					return -1;
				}

				conn_set_owner(c, owner);

				if (pvpgn_v3_send_cdkeyreply3(c,
				                              SERVER_CDKEYREPLY3_MESSAGE_OK,
				                              nullptr) <= 0)
				if ((rpacket = packet_create(packet_class_bnet))) {
					packet_set_size(rpacket, sizeof(t_server_cdkeyreply3));
					packet_set_type(rpacket, SERVER_CDKEYREPLY3);
					bn_int_set(&rpacket->u.server_cdkeyreply3.message, SERVER_CDKEYREPLY3_MESSAGE_OK);
					packet_append_string(rpacket, "");	/* FIXME: owner, message, ??? */
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
				}
			}

			return 0;
		}

}} // namespace pvpgn::bnetd
