/*
 * Copyright (C) 1998  Mark Baysinger (mbaysing@ucsd.edu)
 * Copyright (C) 1998,1999,2000  Ross Combs (rocombs@cs.nmsu.edu)
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

/*
 * anongame_protocol.h — umbrella header (plan 05 §3 / SOLID-S)
 *
 * All anongame packet structs have been split into focused sub-headers by
 * message family.  This file exists only to preserve the original include
 * path used by packet.h and any other consumers; it simply pulls in all
 * five sub-headers so callers need not change their #include directives.
 *
 * Sub-header layout:
 *   bnet_protocol/anongame_protocol_search.h
 *       0x44ff option 00/01 — matchmaking search request and game-found reply
 *       Structs: t_client_anongame, t_client_findanongame,
 *                t_client_findanongame_at_inv, t_client_findanongame_at,
 *                t_server_anongame_search_reply, t_server_anongame_found,
 *                t_saf_pt2
 *       Constants: CLIENT_FINDANONGAME_*, SERVER_FINDANONGAME_SEARCH/FOUND/CANCEL
 *
 *   bnet_protocol/anongame_protocol_info.h
 *       0x44ff option 02 — matchmaking info request/reply
 *       Structs: t_client_findanongame_inforeq, t_server_findanongame_inforeply
 *       Constants: SERVER_ANONGAME_*_STR, CLIENT_FINDANONGAME_INFOTAG_*,
 *                  ANONGAME_TYPE_*, ANONGAME_TYPES
 *
 *   bnet_protocol/anongame_protocol_profile.h
 *       0x44ff options 03/04/07/08 — cancel, player profile, tournament, clan profile
 *       Structs: t_server_findanongame_playgame_cancel,
 *                t_client_findanongame_profile, t_server_findanongame_profile2,
 *                t_client_anongame_tournament_request,
 *                t_server_anongame_tournament_reply,
 *                t_client_findanongame_profile_clan,
 *                t_server_findanongame_profile_clan
 *       Constants: SERVER_FINDANONGAME_PROFILE_UNKNOWN2
 *
 *   bnet_protocol/anongame_protocol_icon.h
 *       0x44ff option 09/0A — icon request/reply
 *       Structs: t_server_findanongame_iconreply
 *
 *   bnet_protocol/anongame_protocol_arranged_team.h
 *       0x60ff–0x63ff — arranged team invite flow
 *       0x65ff–0x69ff — friends list management
 *       Structs: t_client_arrangedteam_friendscreen,
 *                t_server_arrangedteam_friendscreen,
 *                t_client_arrangedteam_invite_friend,
 *                t_server_arrangedteam_invite_friend_ack,
 *                t_server_arrangedteam_send_invite,
 *                t_client_arrangedteam_accept_decline_invite,
 *                t_server_arrangedteam_member_decline,
 *                t_client_friendslistreq, t_server_friendslistreply,
 *                t_server_friendslistreply_status,
 *                t_client_friendinforeq, t_server_friendinforeply,
 *                t_server_friendadd_ack, t_server_frienddel_ack,
 *                t_server_friendmove_ack
 *       Constants: CLIENT/SERVER_ARRANGEDTEAM_*, FRIEND_TYPE_*, FRIENDSTATUS_*
 */
#ifndef INCLUDED_ANONGAME_PROTOCOL_TYPES
#define INCLUDED_ANONGAME_PROTOCOL_TYPES

#include "common/bnet_protocol/anongame_protocol_search.h"
#include "common/bnet_protocol/anongame_protocol_info.h"
#include "common/bnet_protocol/anongame_protocol_profile.h"
#include "common/bnet_protocol/anongame_protocol_icon.h"
#include "common/bnet_protocol/anongame_protocol_arranged_team.h"

#endif /* INCLUDED_ANONGAME_PROTOCOL_TYPES */
