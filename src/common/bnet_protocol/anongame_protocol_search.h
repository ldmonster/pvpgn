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
 * anongame_protocol_search.h — option 00/01: matchmaking search & found
 *
 * Covers 0x44ff option 00 (search request) and option 01 (game found reply).
 * Includes PG, AT, and TY search variants plus the server-found struct.
 *
 * Structs:
 *   t_client_anongame                — base packet (option byte only)
 *   t_client_findanongame            — PG search request
 *   t_client_findanongame_at_inv     — AT inviter search request
 *   t_client_findanongame_at         — AT invitee search request
 *   t_server_anongame_search_reply   — server search acknowledgement
 *   t_server_anongame_found          — server game-found notification
 *   t_saf_pt2                        — appended game-type descriptor
 */

#ifndef INCLUDED_ANONGAME_PROTOCOL_SEARCH
#define INCLUDED_ANONGAME_PROTOCOL_SEARCH

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
	/* first packet received from client - option decides which struct to use next */
#define CLIENT_FINDANONGAME 			0x44ff
#define SERVER_FINDANONGAME 			0x44ff
	typedef struct
	{
		t_bnet_header	h;
		bn_byte		option;
		/* rest of packet data */
	} PACKED_ATTR() t_client_anongame;

	/***********************************************************************************/
#define CLIENT_FINDANONGAME_SEARCH              0x00
#define CLIENT_FINDANONGAME_INFOS               0x02
#define CLIENT_FINDANONGAME_CANCEL		0x03
#define CLIENT_FINDANONGAME_PROFILE             0x04
#define CLIENT_FINDANONGAME_AT_SEARCH           0x05
#define CLIENT_FINDANONGAME_AT_INVITER_SEARCH   0x06
#define CLIENT_ANONGAME_TOURNAMENT		0X07
#define	CLIENT_FINDANONGAME_PROFILE_CLAN	0x08
#define CLIENT_FINDANONGAME_GET_ICON            0x09
#define CLIENT_FINDANONGAME_SET_ICON            0x0A

#define SERVER_FINDANONGAME_SEARCH              0x00
#define SERVER_FINDANONGAME_FOUND		0x01
#define SERVER_FINDANONGAME_CANCEL              0x03

	/***********************************************************************************/
	/* option 00 - anongame search (PG) */
	typedef struct
	{
		t_bnet_header	h;
		bn_byte		option;
		bn_int		count;     /* Goes up each time client clicks search */
		bn_int		unknown2;  /* 00 00 00 00 */
		bn_byte		type;      /* 0 = PG  , 1 = AT  , 2 = TY - from TYPE */
		bn_byte		gametype;  /* 0 = 1v1 , 1 = 2v2 , 2 = 3v3 , 3 = 4v4 , 4 = sffa - from TYPE */
		bn_int		map_prefs; /* map preferences bitmask */
		bn_byte		unknown3;  /* 8 */
		bn_int		id;        /* client id */
		bn_int       	race;      /* 1 = H , 2 = O , 4 = N , 8 = U , 0x20 = R */
	} PACKED_ATTR() t_client_findanongame;

	/* option 05/06 - AT inviter search */
	typedef struct
	{
		t_bnet_header h;
		bn_byte      option;
		bn_int       count;
		bn_int       tid;        /* team id */
		bn_int       timestamp;
		bn_byte	 teamsize;
		bn_int       info[5];   /* client get this info from SERVER_ARRANGEDTEAM_INVITE_FRIEND_ACK (0x61ff) */
		bn_int       unknown2;  /* 00 00 00 00 */
		bn_byte      type;      /* 0 = PG  , 1 = AT  , 2 = TY - from TYPE */
		bn_byte      gametype;  /* 0 = 1v1 , 1 = 2v2 , 2 = 3v3 , 3 = 4v4 , 4 = sffa - from TYPE */
		bn_int       map_prefs; /* map preferences bitmask */
		bn_byte      unknown3;  /* 08 */
		bn_int       id;	    /* client id */
		bn_int       race;      /* 1 = H , 2 = O , 4 = N , 8 = U , 0x20 = R */
	} PACKED_ATTR() t_client_findanongame_at_inv;

	/* option 05 - AT invitee search */
	typedef struct
	{
		t_bnet_header h;
		bn_byte      option;
		bn_int       count;     /* Goes up each time client clicks search */
		bn_int       tid;       /* team id */
		bn_int       timestamp;
		bn_byte      teamsize;
		bn_int       info[5];   /* client get this info from inviter form SERVER_ARRANGEDTEAM_INVITE_FRIEND_ACK (0x61ff) */
		bn_int       unknown2;  /* 00 00 00 00 */
		bn_byte      unknown3;  /* 08 */
		bn_int       id;        /* client id */
		bn_int       race;      /* 1 = H , 2 = O , 4 = N , 8 = U , 0x20 = R */
	} PACKED_ATTR() t_client_findanongame_at;

	/***********************************************************************************/
	/* option 00 - server search acknowledgement */
#define SERVER_ANONGAME_SEARCH_REPLY		0x44ff
	typedef struct
	{
		t_bnet_header h;
		bn_byte   option;
		bn_int    count;
		bn_int    reply;
		/*      bn_short  avgtime; - only in W3XP so far average time in seconds of search */
	} PACKED_ATTR() t_server_anongame_search_reply;

	/***********************************************************************************/
	/* option 01 - anongame found */
#define SERVER_ANONGAME_FOUND			0x44ff
	typedef struct
	{
		t_bnet_header	h;
		bn_byte		option;     /* 1: anongame found */
		bn_int		count;
		bn_int		unknown1;   /* 00 00 00 00 */
		bn_int		ip;
		bn_short		port;
		bn_byte		unknown2;
		bn_byte             unknown3;
		bn_short		unknown4;   /* usually 00 00 , seen 01 00 */
		bn_int		id;         /* random val for identifying client */
		bn_byte		unknown5;   /* 0x06 */
		bn_byte		type;       /* 0 = PG  , 1 = AT  , 2 = TY - from TYPE */
		bn_byte		gametype;   /* for PG - 0 = 1v1 , 1 = 2v2 , 2 = 3v3 , 3 = 4v4 , 4 = sffa
						 * for AT - 0 = 2v2 , 2 = 3v3 , 3 = 4v3
						 * for TY - set to 0
						 * from TYPE
						 */
		/* char *		mapname */
		/* t_saf_pt2 * 	pt2 */
	} PACKED_ATTR() t_server_anongame_found;

	/* MISC PACKET APPEND DATA's */
	typedef struct
	{
		bn_int	unknown1;		/* 0xFFFFFFFF */
		bn_int	anongame_string;	/* ie. SOLO, TEAM, 2VS2, etc. */
		bn_byte	totalplayers;
		bn_byte	totalteams;		/* 1v1 & sffa = 0, rest 2 */
		bn_short	unknown2;		/* 0x0000 */
		bn_byte	visibility;		/* 0x01 = dark - 0x02 = default */
		bn_byte	unknown3;		/* 0x02 */
	} PACKED_ATTR() t_saf_pt2;

}

#endif /* INCLUDED_ANONGAME_PROTOCOL_SEARCH */
