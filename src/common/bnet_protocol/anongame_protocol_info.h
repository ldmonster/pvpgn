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
 * anongame_protocol_info.h — option 02: matchmaking info request/reply
 *
 * Covers 0x44ff option 02 (info request from client and info reply from server).
 * Also contains game-type constants (ANONGAME_TYPE_*) and info-tag constants
 * (CLIENT_FINDANONGAME_INFOTAG_*) and game-string constants
 * (SERVER_ANONGAME_SOLO_STR, etc.).
 *
 * Structs:
 *   t_client_findanongame_inforeq    — client info request
 *   t_server_findanongame_inforeply  — server info reply header
 */

#ifndef INCLUDED_ANONGAME_PROTOCOL_INFO
#define INCLUDED_ANONGAME_PROTOCOL_INFO

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
	/* option 02 - info request */
#define CLIENT_FINDANONGAME_INFOREQ             0x44ff
	typedef struct
	{
		t_bnet_header       h;
		bn_byte             option;         /* type of request:
						 * 0x02 for matchmaking infos */
		bn_int              count;          /* 0x00000001 increments each request of same type */
		bn_byte             noitems;
	} PACKED_ATTR() t_client_findanongame_inforeq;

#define SERVER_FINDANONGAME_INFOREPLY           0x44ff
	typedef struct {
		t_bnet_header       h;
		bn_byte             option; /* as received from client */
		bn_int              count; /* as received from client */
		bn_byte             noitems; /* not very sure about it */
		/* data */
		/*
		for type 0x02 :
		<type of info><unknown int><info>
		if <type of info> is :
		URL\0 : <info> contains 3 NULL terminated urls/strings
		MAP\0 : <info> contains 7 (seen so far) maps names
		TYPE  : <info> unknown 38 bytes probably meaning anongame types
		DESC  : <info>
		*/
	} PACKED_ATTR() t_server_findanongame_inforeply;

	/***********************************************************************************/
	/* Game-string constants (appended in t_saf_pt2::anongame_string) */
#define SERVER_ANONGAME_SOLO_STR        	0x534F4C4F /* "SOLO" */
#define SERVER_ANONGAME_TEAM_STR        	0x5445414D /* "TEAM" */
#define SERVER_ANONGAME_SFFA_STR        	0x46464120 /* "FFA " */
#define SERVER_ANONGAME_AT2v2_STR       	0x32565332 /* "2VS2" */
#define SERVER_ANONGAME_AT3v3_STR       	0x33565333 /* "3VS3" */
#define SERVER_ANONGAME_AT4v4_STR       	0x34565334 /* "4VS4" */
#define SERVER_ANONGAME_TY_STR			0X54592020 /* "TY  " FIXME-TY: WHAT TO PUT HERE */

	/* Info-tag constants (type field in info reply data blocks) */
#define CLIENT_FINDANONGAME_INFOTAG_URL         0x55524c        /*  URL\0 */
#define CLIENT_FINDANONGAME_INFOTAG_MAP         0x4d4150        /*  MAP\0 */
#define CLIENT_FINDANONGAME_INFOTAG_TYPE        0x54595045      /*  TYPE */
#define CLIENT_FINDANONGAME_INFOTAG_DESC        0x44455343      /*  DESC */
#define CLIENT_FINDANONGAME_INFOTAG_LADR        0x4c414452      /*  LADR */
#define CLIENT_FINDANONGAME_INFOTAG_SOLO        0x534f4c4f      /*  SOLO */
#define CLIENT_FINDANONGAME_INFOTAG_TEAM        0x5445414d      /*  TEAM */
#define CLIENT_FINDANONGAME_INFOTAG_FFA         0x46464120      /*  FFA\20 */

	/* Game-type constants (t_client_findanongame::gametype) */
#define ANONGAME_TYPE_1V1       0
#define ANONGAME_TYPE_2V2       1
#define ANONGAME_TYPE_3V3       2
#define ANONGAME_TYPE_4V4       3
#define ANONGAME_TYPE_SMALL_FFA 4
#define ANONGAME_TYPE_AT_2V2    5
#define ANONGAME_TYPE_TEAM_FFA  6
#define ANONGAME_TYPE_AT_3V3    7
#define ANONGAME_TYPE_AT_4V4    8
	/* Added by Omega */
#define ANONGAME_TYPE_TY	9
#define ANONGAME_TYPE_5V5	10
#define ANONGAME_TYPE_6V6	11
#define ANONGAME_TYPE_2V2V2	12
#define ANONGAME_TYPE_3V3V3	13
#define ANONGAME_TYPE_4V4V4	14
#define ANONGAME_TYPE_2V2V2V2	15
#define ANONGAME_TYPE_3V3V3V3	16
#define ANONGAME_TYPE_AT_2V2V2	17

#define ANONGAME_TYPES 18

}

#endif /* INCLUDED_ANONGAME_PROTOCOL_INFO */
