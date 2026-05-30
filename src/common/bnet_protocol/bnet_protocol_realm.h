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

/* Realm packets: GAME_REPORT, JOIN_GAME, STATSUPDATE, REALMJOIN, SEARCH_LAN */
#ifndef INCLUDED_BNET_PROTOCOL_REALM_H
#define INCLUDED_BNET_PROTOCOL_REALM_H

#ifdef JUST_NEED_TYPES
# include "common/bn_type.h"
#else
# define JUST_NEED_TYPES
# include "common/bn_type.h"
# undef JUST_NEED_TYPES
#endif

#include "bnet_protocol/bnet_protocol_common.h"

namespace pvpgn
{

#define CLIENT_GAME_REPORT 0x2cff
	typedef struct
	{
		t_bnet_header h;
		bn_int        unknown1; /* 0x00000000 */
		bn_int        count;    /* (number of player slots (will be 8 for now) */
		/* results... */
		/* names... */
		/* report header */
		/* report body */
	} PACKED_ATTR() t_client_game_report;

	typedef struct
	{
		bn_int result;
	} PACKED_ATTR() t_client_game_report_result;
#define CLIENT_GAME_REPORT_RESULT_PLAYING    0x00000000
#define CLIENT_GAME_REPORT_RESULT_WIN        0x00000001
#define CLIENT_GAME_REPORT_RESULT_LOSS       0x00000002
#define CLIENT_GAME_REPORT_RESULT_DRAW       0x00000003
#define CLIENT_GAME_REPORT_RESULT_DISCONNECT 0x00000004
#define CLIENT_GAME_REPORT_RESULT_OBSERVER   0x00000005
	/******************************************************/


	/******************************************************/
	/*
	War Craft II BNE (original):
	FF 22 1B 00 4E 42 32 57   4B 00 00 00 4C 61 64 64    ."..NB2WK...Ladd
	65 72 20 31 20 6F 6E 20   31 00 00                   er 1 on 1..
	*/
#define CLIENT_JOIN_GAME 0x22ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        clienttag;
		bn_int        versiontag;
		/* game name */
		/* game password */
	} PACKED_ATTR() t_client_join_game;
	/******************************************************/


	/******************************************************/
	/*
	FF 27 84 00 01 00 00 00  04 00 00 00 52 6F 73 73    .'..........Ross
	5F 43 4D 00 70 72 6F 66  69 6C 65 5C 73 65 78 00    _CM.profile\sex.
	70 72 6F 66 69 6C 65 5C  61 67 65 00 70 72 6F 66    profile\age.prof
	69 6C 65 5C 6C 6F 63 61  74 69 6F 6E 00 70 72 6F    ile\location.pro
	66 69 6C 65 5C 64 65 73  63 72 69 70 74 69 6F 6E    file\description
	00 61 73 64 66 00 61 73  66 00 61 73 64 66 00 61    .asdf.asf.asdf.a
	73 64 66 61 73 64 66 61  73 64 66 61 73 64 66 0D    sdfasdfasdfasdf.
	0A 61 73 64 0D 0A 66 61  73 64 0D 0A 66 61 73 64    .asd..fasd..fasd
	66 0D 0A 00                                         f...
	*/
#define CLIENT_STATSUPDATE 0x27ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        name_count;
		bn_int        key_count;
		/* names... */
		/* values... */
	} PACKED_ATTR() t_client_statsupdate;
	/******************************************************/

	/******************************************************/
#define CLIENT_REALMJOINREQ_109 0x3eff
	typedef struct
	{
		t_bnet_header h;
		bn_int        seqno;
		bn_int        seqnohash[5];
		/* Realm Name */
	} PACKED_ATTR() t_client_realmjoinreq_109;
	/******************************************************/

	/******************************************************/
#define SERVER_REALMJOINREPLY_109 0x3eff
	typedef struct
	{
		t_bnet_header h;
		bn_int        seqno;
		bn_int        u1;
		bn_int        bncs_addr1;
		bn_int        sessionnum;
		bn_int        addr;
		bn_short      port;
		bn_short      u3;
		bn_int        sessionkey;     /* zero */
		bn_int        u5;
		bn_int        u6;
		bn_int        clienttag;
		bn_int        versionid;
		bn_int        bncs_addr2;
		bn_int        u7;             /* zero */
		bn_int        secret_hash[5];
		/* account name */
	} PACKED_ATTR() t_server_realmjoinreply_109;
	/******************************************************/

#define CLIENT_SEARCH_LAN_GAMES 0x2ff7
	typedef struct
	{
		t_bnet_header h;
		bn_byte game_tag[4]; /* 3WAR */
		bn_int unknown1;
		bn_int unknown2;
	} PACKED_ATTR() t_client_search_lan_games;

#define CLIENT_CHANGECLIENT	0x5cff
	typedef struct
	{
		t_bnet_header h;
		bn_int clienttag;
	} t_client_changeclient;

#define CLIENT_SETEMAILREPLY		0x59ff
	typedef struct
	{
		t_bnet_header	h;
		/* email address */
	} PACKED_ATTR() t_client_setemailreply;

#define SERVER_SETEMAILREQ		0x59ff
	/* send this packet to client before login ok packet will cause
	client to enter input email screen */
	typedef struct
	{
		t_bnet_header	h;
	} PACKED_ATTR() t_server_setemailreq;

#define CLIENT_GETPASSWORDREQ		0x5aff
	typedef struct
	{
		t_bnet_header	h;
		/* account name */
		/* email address */
	} PACKED_ATTR() t_client_getpasswordreq;

#define CLIENT_CHANGEEMAILREQ		0x5bff
	typedef struct
	{
		t_bnet_header	h;
		/* account name */
		/* old email address */
		/* new email address */
	} PACKED_ATTR() t_client_changeemailreq;

#define CLIENT_MOTDREQ			0x46ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        last_news_time;	/* date of the last news item the client has */
	} PACKED_ATTR() t_client_motdreq;
	/*
	this packet is sent right after cdkey and version auth reply success and crashdump exist
	0x0000: ff 5d 14 00 01 01 00 27   00 0a 01 05 00 00 c0 00    .].....'........
	0x0010: 00 00 00 00                                          ....
	*/

#define CLIENT_CRASHDUMP		0x5dff
	typedef struct
	{
		t_bnet_header	h;
		/* crashdump file data */
		/* contains data like client version, exception code, code address */
	} PACKED_ATTR() t_client_crashdump;

#define CLIENT_CLANINFOREQ		0x82ff
	typedef struct
	{
		t_bnet_header	h;
		bn_int		count;
		bn_int		clantag;
		/* player name */
	} PACKED_ATTR() t_client_claninforeq;

#define SERVER_CLANINFOREPLY		0x82ff
	typedef struct
	{
		t_bnet_header	h;
		bn_int		count;
		bn_byte		fail;
		/* string  - clan name */
		/* bn_byte - clan rank */
		/* bn_int  - join time */
	} PACKED_ATTR() t_server_claninforeply;

} /* namespace pvpgn */

#endif /* INCLUDED_BNET_PROTOCOL_REALM_H */
