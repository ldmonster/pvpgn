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
 * anongame_protocol_arranged_team.h — 0x60ff–0x63ff: arranged team invite flow
 *
 * Covers the full arranged-team invite handshake:
 *   0x60ff — friend screen (list of AT-eligible friends)
 *   0x61ff — invite friend / invite friend ACK
 *   0x62ff — member decline notification
 *   0x63ff — send invite to invitees / accept-decline reply
 *
 * Also covers the friends list messages:
 *   0x65ff — friends list request/reply
 *   0x66ff — friend info request/reply
 *   0x67ff — friend add ACK
 *   0x68ff — friend delete ACK
 *   0x69ff — friend move ACK
 *
 * Structs:
 *   t_client_arrangedteam_friendscreen         — 0x60ff client (empty)
 *   t_server_arrangedteam_friendscreen         — 0x60ff server
 *   t_client_arrangedteam_invite_friend        — 0x61ff client
 *   t_server_arrangedteam_invite_friend_ack    — 0x61ff server
 *   t_server_arrangedteam_send_invite          — 0x63ff server
 *   t_client_arrangedteam_accept_decline_invite — 0x63ff client
 *   t_server_arrangedteam_member_decline       — 0x62ff server
 *   t_client_friendslistreq                    — 0x65ff client
 *   t_server_friendslistreply                  — 0x65ff server
 *   t_server_friendslistreply_status           — status entry in friends list
 *   t_client_friendinforeq                     — 0x66ff client
 *   t_server_friendinforeply                   — 0x66ff server
 *   t_server_friendadd_ack                     — 0x67ff server
 *   t_server_frienddel_ack                     — 0x68ff server
 *   t_server_friendmove_ack                    — 0x69ff server
 */

#ifndef INCLUDED_ANONGAME_PROTOCOL_ARRANGED_TEAM
#define INCLUDED_ANONGAME_PROTOCOL_ARRANGED_TEAM

#ifdef JUST_NEED_TYPES
# include "common/bn_type.h"
#else
# define JUST_NEED_TYPES
# include "common/bn_type.h"
# undef JUST_NEED_TYPES
#endif

namespace pvpgn
{

	/***********************************************************************************/
	/* 0x60ff - arranged team friend screen */
#define CLIENT_ARRANGEDTEAM_FRIENDSCREEN 0x60ff
	typedef struct
	{
		t_bnet_header h;
	} PACKED_ATTR() t_client_arrangedteam_friendscreen;

#define SERVER_ARRANGEDTEAM_FRIENDSCREEN 0x60ff
	typedef struct
	{
		t_bnet_header h;
		bn_byte f_count;
		/* usernames get appended here */
	} PACKED_ATTR() t_server_arrangedteam_friendscreen;

#define SERVER_ARRANGED_TEAM_ADDNAME 0x01

	/***********************************************************************************/
	/* 0x61ff - invite friend */
#define CLIENT_ARRANGEDTEAM_INVITE_FRIEND 0x61ff
	typedef struct
	{
		t_bnet_header	h;
		bn_int		count;
		bn_int		id;
		bn_int		unknown1;	/* 01 00 00 00 */
		bn_byte 	numfriends;	/* next is a byte, that is the number of friends to invite */
		/* usernames get appended here */
	} PACKED_ATTR() t_client_arrangedteam_invite_friend;

#define SERVER_ARRANGEDTEAM_INVITE_FRIEND_ACK 0x61ff
	typedef struct
	{
		t_bnet_header	h;
		bn_int		count;
		bn_int 		id;          /* client id */
		bn_int		timestamp;
		bn_byte 	teamsize;    /* numfriends + 1 */
		bn_int		info[5];
	} PACKED_ATTR() t_server_arrangedteam_invite_friend_ack;

	/***********************************************************************************/
	/* 0x63ff - send invite to invitees / accept-decline */
#define SERVER_ARRANGEDTEAM_SEND_INVITE 0x63ff
	typedef struct
	{
		t_bnet_header h;
		bn_int count;
		bn_int id;          /* client id of inviter */
		bn_int inviterip;   /* IP address of the person who invited them into the game */
		bn_short port;      /* Port of the person who invited them into the game */
		bn_byte numfriends; /* Number of friends that got invited to the game */
		/* username of the inviter */
		/* usernames of the others who got invited */
	} PACKED_ATTR() t_server_arrangedteam_send_invite;

#define CLIENT_ARRANGEDTEAM_ACCEPT_DECLINE_INVITE 0x63ff
	typedef struct
	{
		t_bnet_header h;
		bn_int count;
		bn_int id;
		bn_int option;   /* accept or decline */
		/* username of the inviter */
	} PACKED_ATTR() t_client_arrangedteam_accept_decline_invite;

#define CLIENT_ARRANGEDTEAM_ACCEPT		0x00000003
#define CLIENT_ARRANGEDTEAM_DECLINE		0x00000002

	/***********************************************************************************/
	/* 0x62ff - member decline notification */
#define SERVER_ARRANGEDTEAM_MEMBER_DECLINE 0x62ff
	typedef struct
	{
		t_bnet_header h;
		bn_int count;
		bn_int action; /* number assigned to player? playernum? */
		/* username of the person who declined invitation */
	} PACKED_ATTR() t_server_arrangedteam_member_decline;

#define SERVER_ARRANGEDTEAM_ACCEPT		0x00000003
#define SERVER_ARRANGEDTEAM_DECLINE		0x00000002

	/***********************************************************************************/
	/* 0x65ff - friends list */
#define CLIENT_FRIENDSLISTREQ 0x65ff
	typedef struct
	{
		t_bnet_header h;
	} PACKED_ATTR() t_client_friendslistreq;

#define SERVER_FRIENDSLISTREPLY 0x65ff
	typedef struct
	{
		t_bnet_header h;
		bn_byte friendcount;
		/* 1 byte status, 0-terminated name, 6 bytes unknown, ... */
	} PACKED_ATTR() t_server_friendslistreply;

	typedef struct
	{
		bn_byte status;
		bn_byte location;
		bn_int clienttag;
	} PACKED_ATTR() t_server_friendslistreply_status;

	/***********************************************************************************/
	/* 0x66ff - friend info */
#define CLIENT_FRIENDINFOREQ 0x66ff
	typedef struct
	{
		t_bnet_header h;
		bn_byte friendnum;
	} PACKED_ATTR() t_client_friendinforeq;

#define SERVER_FRIENDINFOREPLY 0x66ff
	typedef struct
	{
		t_bnet_header h;
		bn_byte friendnum;
		bn_byte type;
		bn_byte status;
		bn_int clienttag;
		/* game name */
	} PACKED_ATTR() t_server_friendinforeply;

#define FRIEND_TYPE_NON_MUTUAL 0x00
#define FRIEND_TYPE_MUTUAL     0x01
#define FRIEND_TYPE_DND	       0x02
#define FRIEND_TYPE_AWAY       0x04

	/***********************************************************************************/
	/* 0x67ff - friend add ACK */
#define SERVER_FRIENDADD_ACK 0x67ff
	typedef struct
	{
		t_bnet_header h;
		/* friend name, status */
	} PACKED_ATTR() t_server_friendadd_ack;

	/* 0x68ff - friend delete ACK */
#define SERVER_FRIENDDEL_ACK 0x68ff
	typedef struct
	{
		t_bnet_header h;
		bn_byte friendnum;
	} PACKED_ATTR() t_server_frienddel_ack;

	/* 0x69ff - friend move ACK */
#define SERVER_FRIENDMOVE_ACK 0x69ff
	typedef struct
	{
		t_bnet_header h;
		bn_byte pos1;
		bn_byte pos2;
	} PACKED_ATTR() t_server_friendmove_ack;

#define FRIENDSTATUS_OFFLINE    	0x00
#define FRIENDSTATUS_ONLINE     	0x01
#define FRIENDSTATUS_CHAT       	0x02
#define FRIENDSTATUS_PUBLIC_GAME	0x03
#define FRIENDSTATUS_PRIVATE_GAME	0x05

}

#endif /* INCLUDED_ANONGAME_PROTOCOL_ARRANGED_TEAM */
