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

// R206-split: thin coordinator — keeps only _client_anongame_cancel() and
// handle_anongame_packet() (the SID_FINDANONGAME dispatch switch).
// All other handlers have been extracted into focused translation units:
//   anongame_profile_link.cpp  — _client_anongame_profile / _client_anongame_profile_clan
//   anongame_icon_link.cpp     — check_user_icon / _client_anongame_get_icon / _client_anongame_set_icon
//   anongame_infos_link.cpp    — _client_anongame_infos
//   anongame_tournament_link.cpp — _client_anongame_tournament

#include "common/setup_before.h"
#include "handle_anongame.h"

#include "common/eventlog.h"
#include "common/bn_type.h"
#include "common/packet.h"

#include "anongame.h"
#include "connection.h"
#include "common/setup_after.h"

#include "integration/legacy_bnetd/strangler_macros.h"
#include "integration/legacy_bnetd/send_anongame_cancel_bridge.hpp"

// Observation bridge for SID_FINDANONGAME (0x44) dispatch.
extern "C" int pvpgn_v3_anongame_dispatch(void* conn_ptr,
                                              unsigned int option) noexcept;

namespace pvpgn
{
	namespace bnetd
	{
		// ── Forward declarations for handlers in sub-TUs ──────────────────────
		int _client_anongame_profile(t_connection * c, t_packet const * const packet);
		int _client_anongame_profile_clan(t_connection * c, t_packet const * const packet);
		int _client_anongame_get_icon(t_connection * c, t_packet const * const packet);
		int _client_anongame_set_icon(t_connection * c, t_packet const * const packet);
		int check_user_icon(t_account * account, const char * user_icon);
		int _client_anongame_infos(t_connection * c, t_packet const * const packet);
		int _client_anongame_tournament(t_connection * c, t_packet const * const packet);

		// ── Cancel handler ────────────────────────────────────────────────────
		static int _client_anongame_cancel(t_connection * c)
		{
			t_packet * rpacket;
			t_connection * tc[ANONGAME_MAX_GAMECOUNT / 2];

			// [quetzal] 20020809 - added a_count, so we dont refer to already destroyed anongame
			t_anongame *a = conn_get_anongame(c);
			int a_count, i;

			eventlog(eventlog_level_info, __FUNCTION__, "[{}] got FINDANONGAME CANCEL packet", conn_get_socket(c));

			if (!a)
				return -1;

			a_count = anongame_get_count(a);

			// anongame_unqueue(c, anongame_get_queue(a));
			// -- already doing unqueue in conn_destroy_anongame
			for (i = 0; i < ANONGAME_MAX_GAMECOUNT / 2; i++)
				tc[i] = anongame_get_tc(a, i);

			for (i = 0; i < ANONGAME_MAX_GAMECOUNT / 2; i++) {
				if (tc[i] == NULL)
					continue;

				conn_set_routeconn(tc[i], NULL);
				conn_destroy_anongame(tc[i]);
			}

			// v3 strangler-fig: try the typed pipeline first.
			// `pvpgn_v3_send_anongame_cancel` encodes the 5-byte body
			// (cancel=0x03, count LE) and pushes it via the registered
			// send_packet handler.  Returns 1 on success -> skip legacy path.
			if (pvpgn_v3_send_anongame_cancel(c, static_cast<unsigned int>(a_count)) > 0)
				return 0;

			if (!(rpacket = packet_create(packet_class_bnet)))
				return -1;

			packet_set_size(rpacket, sizeof(t_server_findanongame_playgame_cancel));
			packet_set_type(rpacket, SERVER_FINDANONGAME_PLAYGAME_CANCEL);
			bn_byte_set(&rpacket->u.server_findanongame_playgame_cancel.cancel, SERVER_FINDANONGAME_CANCEL);
			bn_int_set(&rpacket->u.server_findanongame_playgame_cancel.count, a_count);
			conn_push_outqueue(c, rpacket);
			packet_del_ref(rpacket);
			return 0;
		}

		// ── Main dispatch ─────────────────────────────────────────────────────
		extern int handle_anongame_packet(t_connection * c, t_packet const * const packet)
		{
			(void)pvpgn_v3_anongame_dispatch(c,
			    static_cast<unsigned int>(bn_byte_get(packet->u.client_anongame.option)));
			switch (bn_byte_get(packet->u.client_anongame.option))
			{
			case CLIENT_FINDANONGAME_PROFILE:
				return _client_anongame_profile(c, packet);

			case CLIENT_FINDANONGAME_CANCEL:
				return _client_anongame_cancel(c);

			case CLIENT_FINDANONGAME_SEARCH:
			case CLIENT_FINDANONGAME_AT_INVITER_SEARCH:
			case CLIENT_FINDANONGAME_AT_SEARCH:
				return handle_anongame_search(c, packet); /* located in anongame.c */

			case CLIENT_FINDANONGAME_GET_ICON:
				return _client_anongame_get_icon(c, packet);

			case CLIENT_FINDANONGAME_SET_ICON:
				return _client_anongame_set_icon(c, packet);

			case CLIENT_FINDANONGAME_INFOS:
				return _client_anongame_infos(c, packet);

			case CLIENT_ANONGAME_TOURNAMENT:
				return _client_anongame_tournament(c, packet);

			case CLIENT_FINDANONGAME_PROFILE_CLAN:
				return _client_anongame_profile_clan(c, packet);

			default:
				eventlog(eventlog_level_error, __FUNCTION__, "got unhandled option {}", bn_byte_get(packet->u.client_findanongame.option));
				return -1;
			}
		}

	}

}
