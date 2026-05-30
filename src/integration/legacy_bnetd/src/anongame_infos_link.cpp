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

// anongame_infos_link.cpp — game-info/map/URL/type/desc/ladder data handler
// Split from handle_anongame_link.cpp (plan 05 §3 / SOLID-S)
//
// Responsibilities:
//   _client_anongame_infos() — handle SID_FINDANONGAME option 0x02 (INFOS)
//     Dispatches per-item requests: URL, MAP, TYPE, DESC, LADR

#include "common/setup_before.h"
#include "handle_anongame.h"

#include <cstring>

#include "common/eventlog.h"
#include "common/bn_type.h"
#include "common/tag.h"

#include "anongame_infos.h"
#include "server.h"
#include "common/setup_after.h"

#include "integration/legacy_bnetd/strangler_macros.h"

namespace pvpgn
{

	namespace bnetd
	{

		int _client_anongame_infos(t_connection * c, t_packet const * const packet)
		{
			t_packet * rpacket;

			// v3 strangler-fig: try the typed pipeline first. The
			// bridge lazily initialises a snapshot cache from
			// `prefs_get_anongame_infos_file()` + `prefs_get_mapsfile()`,
			// parses the inforeq, resolves per-clienttag/lang, and
			// dispatches the resulting SID 0x44 frames via
			// `conn_push_outqueue`. Returns non-zero only when it has
			// fully handled the request; otherwise we fall back to
			// the legacy path below.
			{
				unsigned int sz = packet_get_size(packet);
				void const*  bd = packet_get_data_const(packet, 0, sz);
				if (bd != nullptr) {
					PVPGN_V3_BRIDGE_TRY(anongame_inforeply, c, bd, sz);
				}
			}

			if (bn_int_get(packet->u.client_findanongame_inforeq.count) > 1) {
				/* reply with 0 entries found */
				int	temp = 0;

				if ((rpacket = packet_create(packet_class_bnet)) == NULL) {
					eventlog(eventlog_level_error, __FUNCTION__, "could not create new packet");
					return -1;
				}

				packet_set_size(rpacket, sizeof(t_server_findanongame_inforeply));
				packet_set_type(rpacket, SERVER_FINDANONGAME_INFOREPLY);
				bn_byte_set(&rpacket->u.server_findanongame_inforeply.option, CLIENT_FINDANONGAME_INFOS);
				bn_int_set(&rpacket->u.server_findanongame_inforeply.count, bn_int_get(packet->u.client_findanongame_inforeq.count));
				bn_byte_set(&rpacket->u.server_findanongame_inforeply.noitems, 0);
				packet_append_data(rpacket, &temp, 1);

				conn_push_outqueue(c, rpacket);
				packet_del_ref(rpacket);
			}
			else {
				int i;
				int client_tag;
				int server_tag_count = 0;
				int client_tag_unk;
				int server_tag_unk;
				bn_int temp;
				char noitems;
				char * tmpdata;
				int tmplen;
				t_clienttag clienttag = conn_get_clienttag(c);
				char last_packet = 0x00;
				char other_packet = 0x01;
				char langstr[5];
				t_gamelang gamelang = conn_get_gamelang(c);
				bn_int_tag_get((bn_int const *)&gamelang, langstr, 5);

				/* Send seperate packet for each item requested
				 * sending all at once overloaded w3xp
				 * [Omega] */
				for (i = 0; i < bn_byte_get(packet->u.client_findanongame_inforeq.noitems); i++){
					noitems = 0;

					if ((rpacket = packet_create(packet_class_bnet)) == NULL) {
						eventlog(eventlog_level_error, __FUNCTION__, "could not create new packet");
						return -1;
					}

					/* Starting the packet stuff */
					packet_set_size(rpacket, sizeof(t_server_findanongame_inforeply));
					packet_set_type(rpacket, SERVER_FINDANONGAME_INFOREPLY);
					bn_byte_set(&rpacket->u.server_findanongame_inforeply.option, CLIENT_FINDANONGAME_INFOS);
					bn_int_set(&rpacket->u.server_findanongame_inforeply.count, 1);

					std::memcpy(&temp, (packet_get_data_const(packet, 10 + (i * 8), 4)), sizeof(int));
					client_tag = bn_int_get(temp);
					std::memcpy(&temp, packet_get_data_const(packet, 14 + (i * 8), 4), sizeof(int));
					client_tag_unk = bn_int_get(temp);

					switch (client_tag){
					case CLIENT_FINDANONGAME_INFOTAG_URL:
						bn_int_set((bn_int*)&server_tag_unk, 0xBF1F1047);
						packet_append_data(rpacket, "LRU\0", 4);
						packet_append_data(rpacket, &server_tag_unk, 4);
						// FIXME: Maybe need do do some checks to avoid prefs empty strings.
						tmpdata = anongame_infos_data_get_url(clienttag, conn_get_versionid(c), &tmplen);
						packet_append_data(rpacket, tmpdata, tmplen);
						noitems++;
						server_tag_count++;
						eventlog(eventlog_level_debug, __FUNCTION__, "client_tag request tagid=(0x{:01x}) tag=({})  tag_unk=(0x{:04x})", i, "CLIENT_FINDANONGAME_INFOTAG_URL", client_tag_unk);
						break;
					case CLIENT_FINDANONGAME_INFOTAG_MAP:
						bn_int_set((bn_int*)&server_tag_unk, 0x70E2E0D5);
						packet_append_data(rpacket, "PAM\0", 4);
						packet_append_data(rpacket, &server_tag_unk, 4);
						tmpdata = anongame_infos_data_get_map(clienttag, conn_get_versionid(c), &tmplen);
						packet_append_data(rpacket, tmpdata, tmplen);
						noitems++;
						server_tag_count++;
						eventlog(eventlog_level_debug, __FUNCTION__, "client_tag request tagid=(0x{:01x}) tag=({})  tag_unk=(0x{:04x})", i, "CLIENT_FINDANONGAME_INFOTAG_MAP", client_tag_unk);
						break;
					case CLIENT_FINDANONGAME_INFOTAG_TYPE:
						bn_int_set((bn_int*)&server_tag_unk, 0x7C87DEEE);
						packet_append_data(rpacket, "EPYT", 4);
						packet_append_data(rpacket, &server_tag_unk, 4);
						tmpdata = anongame_infos_data_get_type(clienttag, conn_get_versionid(c), &tmplen);
						packet_append_data(rpacket, tmpdata, tmplen);
						noitems++;
						server_tag_count++;
						eventlog(eventlog_level_debug, __FUNCTION__, "client_tag request tagid=(0x{:01x}) tag=({}) tag_unk=(0x{:04x})", i, "CLIENT_FINDANONGAME_INFOTAG_TYPE", client_tag_unk);
						break;
					case CLIENT_FINDANONGAME_INFOTAG_DESC:
						bn_int_set((bn_int*)&server_tag_unk, 0xA4F0A22F);
						packet_append_data(rpacket, "CSED", 4);
						packet_append_data(rpacket, &server_tag_unk, 4);
						tmpdata = anongame_infos_data_get_desc(langstr, clienttag, conn_get_versionid(c), &tmplen);
						packet_append_data(rpacket, tmpdata, tmplen);
						eventlog(eventlog_level_debug, __FUNCTION__, "client_tag request tagid=(0x{:01x}) tag=({}) tag_unk=(0x{:04x})", i, "CLIENT_FINDANONGAME_INFOTAG_DESC", client_tag_unk);
						noitems++;
						server_tag_count++;
						break;
					case CLIENT_FINDANONGAME_INFOTAG_LADR:
						bn_int_set((bn_int*)&server_tag_unk, 0x3BADE25A);
						packet_append_data(rpacket, "RDAL", 4);
						packet_append_data(rpacket, &server_tag_unk, 4);
						tmpdata = anongame_infos_data_get_ladr(langstr, clienttag, conn_get_versionid(c), &tmplen);
						packet_append_data(rpacket, tmpdata, tmplen);
						noitems++;
						server_tag_count++;
						eventlog(eventlog_level_debug, __FUNCTION__, "client_tag request tagid=(0x{:01x}) tag=({}) tag_unk=(0x{:04x})", i, "CLIENT_FINDANONGAME_INFOTAG_LADR", client_tag_unk);
						break;
					default:
						eventlog(eventlog_level_debug, __FUNCTION__, "unrec client_tag request tagid=(0x{:01x}) tag=(0x{:04x})", i, client_tag);

					}
					//Adding a last padding null-byte
					if (server_tag_count == bn_byte_get(packet->u.client_findanongame_inforeq.noitems))
						packet_append_data(rpacket, &last_packet, 1); /* only last packet in group gets 0x00 */
					else
						packet_append_data(rpacket, &other_packet, 1); /* the rest get 0x01 */

					//Go,go,go
					bn_byte_set(&rpacket->u.server_findanongame_inforeply.noitems, noitems);
					conn_push_outqueue(c, rpacket);
					packet_del_ref(rpacket);
				}
			}
			return 0;
		}

	} // namespace bnetd

} // namespace pvpgn
