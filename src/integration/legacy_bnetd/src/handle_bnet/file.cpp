// =====================================================================
// Auto-split from handle_bnet_link.cpp by scripts/dev/split_handle_bnet.py
// See plans/15-large-file-decomposition-detail.md for rationale.
// =====================================================================
#include "handle_bnet_internal.h"

namespace pvpgn { namespace bnetd {

		int _client_udpok(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_telemetry_dispatch(c, "udpok");
			if (packet_get_size(packet) < sizeof(t_client_udpok)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad UDPOK packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_udpok), packet_get_size(packet));
				return -1;
			}
			/* we could check the contents but there really isn't any point */
			conn_set_udpok(c);

			return 0;
		}

		int _client_fileinforeq(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_telemetry_dispatch(c, "fileinforeq");
			t_packet *rpacket;

			if (packet_get_size(packet) < sizeof(t_client_fileinforeq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad FILEINFOREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_fileinforeq), packet_get_size(packet));
				return -1;
			}

			{
				char const *filename;

				if (!(filename = packet_get_str_const(packet, sizeof(t_client_fileinforeq), MAX_FILENAME_STR))) {
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad FILEINFOREQ packet (missing or too long tosfile)", conn_get_socket(c));
					return -1;
				}
				eventlog(eventlog_level_info, __FUNCTION__, "[{}] file requested: \"{}\" - type = 0x{:02x}", conn_get_socket(c), filename, bn_int_get(packet->u.client_fileinforeq.type));

				/* TODO: if type is TOSFILE make bnetd to send default tosfile if selected is not found */
				if ((rpacket = packet_create(packet_class_bnet))) {
					packet_set_size(rpacket, sizeof(t_server_fileinforeply));
					packet_set_type(rpacket, SERVER_FILEINFOREPLY);
					bn_int_set(&rpacket->u.server_fileinforeply.type, bn_int_get(packet->u.client_fileinforeq.type));
					bn_int_set(&rpacket->u.server_fileinforeply.unknown2, bn_int_get(packet->u.client_fileinforeq.unknown2));
					/* Note from Sherpya:
					 * timestamp -> 0x852b7d00 - 0x01c0e863 b.net send this (bn_int),
					 * I suppose is not a long
					 * if bnserver-D2DV is bad diablo 2 crashes
					 * timestamp doesn't work correctly and starcraft
					 * needs name in client locale or displays hostname
					 */
					file_to_mod_time(c, filename, &rpacket->u.server_fileinforeply.timestamp);
					packet_append_string(rpacket, filename);
					{
						const unsigned long long v3_ts =
							bn_long_get(rpacket->u.server_fileinforeply.timestamp);
						if (pvpgn_v3_send_fileinforeply(
								c,
								static_cast<std::uint32_t>(bn_int_get(rpacket->u.server_fileinforeply.type)),
								static_cast<std::uint32_t>(bn_int_get(rpacket->u.server_fileinforeply.unknown2)),
								static_cast<std::uint64_t>(v3_ts),
								filename) == 1) {
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

		int _client_statsreq(t_connection * c, t_packet const *const packet)
		{
			(void)pvpgn_v3_profile_dispatch(c, "statsreq");
			t_packet *rpacket;
			char const *name;
			char const *key;
			unsigned int name_count;
			unsigned int key_count;
			unsigned int i, j;
			unsigned int name_off;
			unsigned int keys_off;
			unsigned int key_off;
			t_account *reqacc, *myacc;

			if (packet_get_size(packet) < sizeof(t_client_statsreq)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STATSREQ packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_statsreq), packet_get_size(packet));
				return -1;
			}

			name_count = bn_int_get(packet->u.client_statsreq.name_count);
			key_count = bn_int_get(packet->u.client_statsreq.key_count);

			for (i = 0, name_off = sizeof(t_client_statsreq); i < name_count && (name = packet_get_str_const(packet, name_off, UNCHECKED_NAME_STR)); i++, name_off += std::strlen(name) + 1);

			if (i < name_count) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad STATSREQ packet (only {} names of {})", conn_get_socket(c), i, name_count);
				return -1;
			}
			keys_off = name_off;

			if (!(rpacket = packet_create(packet_class_bnet)))
				return -1;

			packet_set_size(rpacket, sizeof(t_server_statsreply));
			packet_set_type(rpacket, SERVER_STATSREPLY);
			bn_int_set(&rpacket->u.server_statsreply.name_count, name_count);
			bn_int_set(&rpacket->u.server_statsreply.key_count, key_count);
			bn_int_set(&rpacket->u.server_statsreply.requestid, bn_int_get(packet->u.client_statsreq.requestid));

			myacc = conn_get_account(c);

			for (i = 0, name_off = sizeof(t_client_statsreq); i < name_count && (name = packet_get_str_const(packet, name_off, UNCHECKED_NAME_STR)); i++, name_off += std::strlen(name) + 1) {
				reqacc = accountlist_find_account(name);
				if (!reqacc)
					reqacc = myacc;

				for (j = 0, key_off = keys_off; j < key_count && (key = packet_get_str_const(packet, key_off, MAX_ATTRKEY_STR)); j++, key_off += std::strlen(key) + 1) {
					if (*key == '\0')
						continue;
					packet_append_string(rpacket, _attribute_req(reqacc, myacc, key));
				}
			}

			{
				// Strangler-fig: collect the appended value strings
				// from the legacy-built rpacket and ship via v3 codec.
				// The values start immediately after the fixed header
				// (sizeof(t_server_statsreply) bytes).
				unsigned int v3_req_id = bn_int_get(
					rpacket->u.server_statsreply.requestid);
				unsigned int v3_pkt_size = packet_get_size(rpacket);
				unsigned int v3_fixed = static_cast<unsigned int>(
					sizeof(t_server_statsreply));
				// Build a flat array of C-string pointers into the
				// packet payload for the bridge.
				std::vector<char const*> v3_vals;
				if (v3_pkt_size > v3_fixed) {
					char const* p = static_cast<char const*>(
						packet_get_data_const(rpacket, v3_fixed,
							v3_pkt_size - v3_fixed));
					char const* end = p + (v3_pkt_size - v3_fixed);
					while (p && p < end) {
						v3_vals.push_back(p);
						std::size_t slen = std::strlen(p);
						p += slen + 1;
					}
				}
				int v3rc = pvpgn_v3_send_statsreply(
					c,
					name_count,
					key_count,
					v3_req_id,
					v3_vals.empty() ? nullptr : v3_vals.data(),
					static_cast<unsigned int>(v3_vals.size()));
				if (v3rc == 1) {
					packet_del_ref(rpacket);
					return 0;
				}
			}
			conn_push_outqueue(c, rpacket);
			packet_del_ref(rpacket);

			return 0;
		}

		int _client_readmemory(t_connection * c, t_packet const *const packet)
		{
			unsigned int size, offset, request_id;

			if (packet_get_size(packet) < sizeof(t_client_readmemory)) {
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad READMEMORY packet (expected {} bytes, got {})", conn_get_socket(c), sizeof(t_client_readmemory), packet_get_size(packet));
				return -1;
			}
 
			request_id = bn_int_get(packet->u.client_readmemory.request_id);

			size = (unsigned int)packet_get_size(packet);
			offset = sizeof(t_client_readmemory);

			eventlog(eventlog_level_debug, __FUNCTION__, "[{}] Received READMEMORY packet with Request ID: {} and Memory size: {}", conn_get_socket(c), request_id, size - offset);

#ifdef WITH_LUA
			std::vector<int> _data;
			for (int i = offset; i < size; i++)
			{
				_data.push_back(packet->u.data[i]);
			}
			lua_handle_client_readmemory(c, request_id, _data);
#endif

			return 0;
		}


}} // namespace pvpgn::bnetd
