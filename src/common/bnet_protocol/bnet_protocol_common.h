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

/* Common types: t_bnet_header, t_w3route_header, W3Route packets */
#ifndef INCLUDED_BNET_PROTOCOL_COMMON_H
#define INCLUDED_BNET_PROTOCOL_COMMON_H

#ifdef JUST_NEED_TYPES
# include "common/bn_type.h"
#else
# define JUST_NEED_TYPES
# include "common/bn_type.h"
# undef JUST_NEED_TYPES
#endif

namespace pvpgn
{


	/******************************************************/
	typedef struct
	{
		bn_short type;
		bn_short size;
	} PACKED_ATTR() t_bnet_header;
	/******************************************************/

	/* [zap-zero] 20020529 - added support for war3 anongame routing packets */
	/******************************************************/
	typedef struct
	{
		bn_short type;
		bn_short size;
	} PACKED_ATTR() t_w3route_header;
	/******************************************************/


	/******************************************************/
	typedef struct
	{
		t_bnet_header h;
	} PACKED_ATTR() t_bnet_generic;
	/******************************************************/

	/******************************************************/
	typedef struct
	{
		t_w3route_header h;
	} PACKED_ATTR() t_w3route_generic;
	/******************************************************/

	/* for unhandled pmap packets */
#define CLIENT_NULL 0xfeff


	/******************************************************/
	/*
	14: recv class=w3route[0x0c] type=unknown[0x00f7] length=53
	0000:   F7 1E 35 00 02 02 00 00   68 BF 0B 08 00 E0 17 01    ..5.....h.......
	0010:   00 00 00 42 6C 61 63 6B   72 61 74 00 08 81 3E 0C    ...Blackrat...>.
	0020:   00 20 00 00 00 02 00 17   E1 C0 A8 00 01 00 00 00    . ..............
	0030:   00 00 00 00 00                                       .....
	*/
#define CLIENT_W3ROUTE_REQ 0x1ef7
	typedef struct
	{
		t_w3route_header h;
		bn_int unknown1;
		bn_int id;			/* unique id sent by server */
		bn_byte unknown2;
		bn_short port;
		bn_int handle;		/* handle/descriptor for client <-> client communication */
		/* player name, ... */
	} PACKED_ATTR() t_client_w3route_req;
	/******************************************************/



	/******************************************************/
	/*
	f7 23 04 00
	*/
#define CLIENT_W3ROUTE_LOADINGDONE 0x23f7
	typedef struct
	{
		t_w3route_header h;
	} PACKED_ATTR() t_client_w3route_loadingdone;
	/******************************************************/


	/******************************************************/
	/*
	f7 14 05 00 00
	*/
#define SERVER_W3ROUTE_READY 0x14f7
	typedef struct
	{
		t_w3route_header h;
		bn_byte unknown1;
	} PACKED_ATTR() t_server_w3route_ready;
	/******************************************************/


	/******************************************************/
	/*
	f7 21 08 00 01 00 00 00
	*/

#define CLIENT_W3ROUTE_ABORT 0x21f7

	typedef struct
	{
		t_w3route_header h;
		bn_int unknown1;       /* count? */
	} PACKED_ATTR() t_client_w3route_abort;

	/******************************************************/
	/*
	f7 08 05 00 02
	*/
#define SERVER_W3ROUTE_LOADINGACK 0x08f7
	typedef struct
	{
		t_w3route_header h;
		bn_byte playernum;
	} PACKED_ATTR() t_server_w3route_loadingack;
	/******************************************************/


	/******************************************************/
	/*
	f7 3b 06 00 09 00
	*/
#define CLIENT_W3ROUTE_CONNECTED 0x3bf7
	typedef struct
	{
		t_w3route_header h;
		bn_short unknown1;
	} PACKED_ATTR() t_client_w3route_connected;
	/******************************************************/

	/******************************************************/
	/*
	f7 01 08 00 2e 85 2d 7b
	*/
#define SERVER_W3ROUTE_ECHOREQ 0x01f7
	typedef struct
	{
		t_w3route_header h;
		bn_int ticks;
	} PACKED_ATTR() t_server_w3route_echoreq;
	/******************************************************/


#define CLIENT_W3ROUTE_ECHOREPLY 0x46f7


	/******************************************************/
	/*
	13: recv class=w3route[0x0c] type=unknown[0x2ef7] length=107
	0000:   F7 2E 6B 00 01 01 03 00   00 00 20 00 00 00 5C 01    ..k....... ...\.
	0010:   00 00 00 05 00 00 00 00   00 00 00 00 00 00 00 1A    ................
	0020:   04 00 00 00 00 00 00 00   00 00 00 05 00 00 00 00    ................
	0030:   00 00 00 01 00 00 00 00   00 00 00 05 00 00 00 00    ................
	0040:   00 00 00 00 00 00 00 00   00 00 00 00 00 00 00 00    ................
	0050:   00 00 00 00 00 00 00 00   00 00 00 00 00 00 00 00    ................
	0060:   00 00 00 00 00 00 00 00   00 00 00                   ...........
	*/
#define CLIENT_W3ROUTE_GAMERESULT 0x2ef7
	/*
							   F7 3A-84 00 02 01 04 00 00 00
							   0x0040   20 00 00 00 83 09 00 00-00 00 00 00 02 03 00 00
							   0x0050   00 20 00 00 00 2C 01 00-00 00 00 00 00 5C 04 00
							   0x0060   00 00 03 00 00 00 00 00-00 00 01 00 00 00 CC 0B
							   0x0070   00 00 00 00 00 00 72 0B-00 00 0E 00 00 00 00 00
							   0x0080   00 00 04 00 00 00 01 00-00 00 15 00 00 00 00 00
							   0x0090   00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00
							   0x00A0   00 00 46 0A 00 00 2C 01-00 00 00 00 00 00 00 00
							   0x00B0   00 00 00 00 00 00 00 00-00 00

							   F7 3A-84 00 02 02 03 00 00 00
							   0x0040   20 00 00 00 2C 01 00 00-00 00 00 00 01 04 00 00
							   0x0050   00 20 00 00 00 83 09 00-00 00 00 00 00 5C 04 00
							   0x0060   00 00 01 00 00 00 01 00-00 00 00 00 00 00 39 03
							   0x0070   00 00 00 00 00 00 00 00-00 00 05 00 00 00 00 00
							   0x0080   00 00 01 00 00 00 00 00-00 00 05 00 00 00 00 00
							   0x0090   00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00
							   0x00A0   00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00
							   0x00B0   00 00 00 00 00 00 00 00-00 00
							   */

#define CLIENT_W3ROUTE_GAMERESULT_W3XP 0x3af7
	typedef struct
	{
		t_w3route_header h;
		bn_byte number_of_results;
		/*
				t_client_w3route_gameresult_player players[];   1-n times
				t_client_w3route_gameresult_part2  part2;
				t_client_w3route_gameresult_hero   heroes[];    0-n times
				t_client_w3route_gameresult_part3  part3; */
	} PACKED_ATTR() t_client_w3route_gameresult;
	/******************************************************/

	/* [zap-zero] what value is DRAW? */
#define W3_GAMERESULT_LOSS	0x00000003
#define W3_GAMERESULT_WIN	0x00000004

	typedef struct
	{
		bn_byte number;
		bn_int  result;
		bn_int  race;
		bn_int  unknown1;
		bn_int  unknown2;
	} PACKED_ATTR() t_client_w3route_gameresult_player;

	typedef struct
	{
		bn_byte unknown0; /* but could also contain info about result */
		bn_int  unknown1;
		bn_int  unknown2;
		bn_int  unknown3;
		bn_int  unknown4;
		bn_int  unit_score;
		bn_int  heroes_score;
		bn_int  resource_score;
		bn_int  units_produced;
		bn_int  units_killed;
		bn_int  buildings_produced;
		bn_int  buildings_razed;
		bn_int  largest_army;
		bn_int  heroes_used_count;
	} PACKED_ATTR() t_client_w3route_gameresult_part2;

	typedef struct
	{
		bn_short level;
		bn_int  race_and_name;
		bn_int  hero_xp;
	} PACKED_ATTR() t_client_w3route_gameresult_hero;

	typedef struct
	{
		bn_int  heroes_killed;
		bn_int  items_obtained;
		bn_int  mercenaries_hired;
		bn_int  total_hero_xp;
		bn_int  gold_mined;
		bn_int  lumber_harvested;
		bn_int  resources_traded_given;
		bn_int  resources_traded_taken;
		bn_int  tech_percentage;
		bn_int  gold_lost_to_upkeep;
	} PACKED_ATTR() t_client_w3route_gameresult_part3;

	/******************************************************/
	/*
							f7 04 1e 00 07 00 00 b2 72 76   ..............rv
							0040  ec cc cc 02 02 00 04 0b d9 e9 5f ac 00 00 00 00   .........._.....
							0050  00 00 00 00                                       ....
							*/
#define SERVER_W3ROUTE_ACK 0x04f7
	typedef struct
	{
		t_w3route_header h;
		bn_byte unknown1;	/* 07 */
		bn_short unknown2;	/* 00 00 */
		bn_int unknown3;	/* random stuff */
		bn_short unknown4;	/* cc cc */
		bn_byte playernum;	/* 1-4 */
		bn_short unknown5;	/* 0x0002 */
		bn_short port;		/* client port */
		bn_int ip;		/* client ip */
		bn_int unknown7;	/* 00 00 00 00 */
		bn_int unknown8;	/* 00 00 00 00 */
	} PACKED_ATTR() t_server_w3route_ack;
	/******************************************************/

#define SERVER_W3ROUTE_ACK_UNKNOWN3	0x484e2637

	/******************************************************/
	/*
							f7 06 3b 00 01 00 00 00 01 **   ........;......P
							0040  ** ** ** ** ** ** ** 00 08 2d 09 9d 04 08 00 00   layer00..-......
							0050  00 02 00 17 e0 [censored]  00 00 00 00 00 00 00   .......g........
							0060  00 02 00 17 e0 [censored]  00 00 00 00 00 00 00   .......g........
							0070  00

							*/
#define SERVER_W3ROUTE_PLAYERINFO 0x06f7
	typedef struct
	{
		t_w3route_header h;
		bn_int handle;
		bn_byte playernum;
		/* then:
		   opponent name
		   playerinfo2
		   playerinfo_addr (external addr)
		   playerinfo_addr (local lan addr)
		   */
	} PACKED_ATTR() t_server_w3route_playerinfo;

	typedef struct
	{
		bn_byte unknown1;		/* 8 (length?) */
		bn_int id;			/* id from FINDANONGAME_SEARCH packet */
		bn_int race;			/* see defines */
	} PACKED_ATTR() t_server_w3route_playerinfo2;

#define SERVER_W3ROUTE_LEVELINFO 0x47f7
	typedef struct
	{
		t_w3route_header h;
		bn_byte numplayers;
		/* then: levelinfo2 for each player */
	} PACKED_ATTR() t_server_w3route_levelinfo;

	typedef struct
	{
		bn_byte plnum;
		bn_byte unknown1;		/* 3 (length?) */
		bn_byte level;
		bn_short unknown2;
	} PACKED_ATTR() t_server_w3route_levelinfo2;


	typedef struct
	{
		bn_short unknown1;		/* 2 */
		bn_short port;
		bn_int ip;
		bn_int unknown2;		/* 0 */
		bn_int unknown3;		/* 0 */
	} PACKED_ATTR() t_server_w3route_playerinfo_addr;


	typedef struct
	{
		t_w3route_header h;	/* f7 0a 04 00 */
	} PACKED_ATTR() t_server_w3route_startgame1;
#define SERVER_W3ROUTE_STARTGAME1 0x0af7

	typedef struct
	{
		t_w3route_header h;	/* f7 0b 04 00 */
	} PACKED_ATTR() t_server_w3route_startgame2;
#define SERVER_W3ROUTE_STARTGAME2 0x0bf7

	/*******************************************************/
	/*
		ALL ANONGAME PACKET INFO (0x44ff) MOVED TO anongame_protocol.h
		[Omega]
		*/
	/******************************************************/
	/*
	These two dumps are from the original unpatched Starcraft client:

	FF 05 28 00 01 00 00 00            ..(.....
	D1 43 88 AA DA 9D 1B 00   9A F7 69 AB 4A 41 32 30    .C........i.JA20
	35 43 2D 30 34 00 6C 61   62 61 73 73 69 73 74 00    5C-04.labassist.

	FF 05 24 00 01 00 00 00   D1 43 88 AA DA 9D 1B 00    ..$......C......
	9A F7 69 AB 42 4F 42 20   20 20 20 20 20 20 20 00    ..i.BOB        .
	42 6F 62 00                                          Bob.

	??? note it sends NO host and user strings
	FF 05 14 00 01 00 00 00   D8 94 F6 07 B3 2C 6E 02    .............,n.
	B4 E0 3B 6C                                          ..;l
	??? sent right after it... request for session key?
	FF 28 08 00   F6 0F 08 00                    .(......

	Diablo II 1.03 ... note it sends NO host and user strings
	FF 05 14 00 01 00   00 00 D1 43 88 AA DA 9D      ...... ...C....
	1B 00 9A F7 69 AB                                    ....i.
	*/

} /* namespace pvpgn */

#endif /* INCLUDED_BNET_PROTOCOL_COMMON_H */
