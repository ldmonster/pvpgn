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

/* Chat game-list packets: GAMELISTREQ, GAMELISTREPLY */
#ifndef INCLUDED_BNET_PROTOCOL_CHAT_GAME_H
#define INCLUDED_BNET_PROTOCOL_CHAT_GAME_H

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

	/******************************************************/
#define CLIENT_GAMELISTREQ 0x09ff
	typedef struct
	{
		t_bnet_header h;
		bn_short      gametype;
		bn_short      unknown1;
		bn_int        unknown2;
		bn_int        unknown3;
		bn_int        maxgames;
		/* game name */
	} PACKED_ATTR() t_client_gamelistreq;
#define CLIENT_GAMELISTREQ_ALL       0x0000
#define CLIENT_GAMELISTREQ_MELEE     0x0002
#define CLIENT_GAMELISTREQ_FFA       0x0003
#define CLIENT_GAMELISTREQ_ONEONONE  0x0004
#define CLIENT_GAMELISTREQ_CTF       0x0005
#define CLIENT_GAMELISTREQ_GREED     0x0006
#define CLIENT_GAMELISTREQ_SLAUGHTER 0x0007
#define CLIENT_GAMELISTREQ_SDEATH    0x0008
#define CLIENT_GAMELISTREQ_LADDER    0x0009
#define CLIENT_GAMELISTREQ_IRONMAN   0x0010
#define CLIENT_GAMELISTREQ_MAPSET    0x000a
#define CLIENT_GAMELISTREQ_TEAMMELEE 0x000b
#define CLIENT_GAMELISTREQ_TEAMFFA   0x000c
#define CLIENT_GAMELISTREQ_TEAMCTF   0x000d
#define CLIENT_GAMELISTREQ_PGL       0x000e
#define CLIENT_GAMELISTREQ_TOPVBOT   0x000f
#define CLIENT_GAMELISTREQ_DIABLO    0x0409 /* FIXME: this should be the langid */
#define CLIENT_GAMELISTREQ_LOADED    0x0a00
	/* FIXME: Diablo reports differently than it is listed in GAMELIST */
#define CLIENT_GAMETYPE_DIABLO_0     0x00000000 /* Level 1 Char */
#define CLIENT_GAMETYPE_DIABLO_1     0x00000001 /* Level 2 Char */
#define CLIENT_GAMETYPE_DIABLO_2     0x00000002 /* Level ? Char */
#define CLIENT_GAMETYPE_DIABLO_3     0x00000003 /* Level 4 Char */
#define CLIENT_GAMETYPE_DIABLO_4     0x00000004 /* Level ? Char */
#define CLIENT_GAMETYPE_DIABLO_5     0x00000005 /* Level ? Char */
#define CLIENT_GAMETYPE_DIABLO_6     0x00000006 /* Level ? Char */
#define CLIENT_GAMETYPE_DIABLO_7     0x00000007 /* Level ? Char */
#define CLIENT_GAMETYPE_DIABLO_8     0x00000008 /* Level 20-24 Char */
#define CLIENT_GAMETYPE_DIABLO_9     0x00000009 /* Level 25-? Char */
#define CLIENT_GAMETYPE_DIABLO_a     0x0000000a /* Level ? Char */
#define CLIENT_GAMETYPE_DIABLO_b     0x0000000b /* Level ? Char */
#define CLIENT_GAMETYPE_DIABLO_c     0x0000000c /* Level ? Char */
#define CLIENT_GAMETYPE_DIABLO_d     0x0000000d /* Level ? Char */
	/* list might continue - what is maximum diablo level ? */
	/* FIXME: Not sure how Diablo II does things yet */
#define CLIENT_GAMETYPE_DIABLO2_CLOSE 		0x00000000 /* close game */
#define CLIENT_GAMETYPE_DIABLO2_OPEN_NORMAL	0X00000008 /* open, normal difficulty */
#define CLIENT_GAMETYPE_DIABLO2_OPEN_NIGHTMARE	0X00000009 /* open, nightmare difficulty */
#define CLIENT_GAMETYPE_DIABLO2_OPEN_HELL	0X0000000a /* open, hell difficulty */
#define CLIENT_GAMETYPE_DIABLO2_OPEN_HARDCORE_NORMAL	0X0000000c /* open, hardcore, normal difficulty */
#define CLIENT_GAMETYPE_DIABLO2_OPEN_HARDCORE_NIGHTMARE	0X0000000d /* open, hardcore, nightmare difficulty */
#define CLIENT_GAMETYPE_DIABLO2_OPEN_HARDCORE_HELL	0X0000000e /* open, hardcore, hell difficulty */
	/******************************************************/


	/******************************************************/
#define SERVER_GAMELISTREPLY 0x09ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        gamecount;
		bn_int	  sstatus; /* when reply with error to specific game */
		/* games */
	} PACKED_ATTR() t_server_gamelistreply;

#define SERVER_GAMELISTREPLY_GAME_SSTATUS_NOTFOUND	0x0 /* but also any other value diff from the ones bellow */
#define SERVER_GAMELISTREPLY_GAME_SSTATUS_PASS		0x2 /* password incorrect */
#define SERVER_GAMELISTREPLY_GAME_SSTATUS_FULL		0x3 /* game full */
#define SERVER_GAMELISTREPLY_GAME_SSTATUS_STARTED	0x4 /* game started */
#define SERVER_GAMELISTREPLY_GAME_SSTATUS_NOSPAWNCDKEY	0x5 /* trying to use a spawn install join invalid cdkey creator game */
#define SERVER_GAMELISTREPLY_GAME_SSTATUS_LOADED        0x0a00 /* Loaded game */

	typedef struct
	{
		/*	if yak doesn't like this... then the client doesn't also =) (bbf)
			bn_int   unknown7; */
		bn_short gametype;
		bn_short unknown1; /* langid under Diablo... */
		bn_short unknown3;
		/*  bn_int   deleted; */ /* they changed the structure at one point */
		bn_short port;     /* big endian byte order... at least they are consistent! */
		bn_int   game_ip;  /* big endian byte order */
		bn_int   unknown4;
		bn_int   unknown5; /* FIXME: got to figure out where latency is */
		bn_int   status;
		bn_int   unknown6;
		/* game name */
		/* clear password */
		/* info */
	} PACKED_ATTR() t_server_gamelistreply_game;
#define SERVER_GAMELISTREPLY_GAME_UNKNOWN7       0x00000000 //0x00000409 // 0x0000000c
#define SERVER_GAMELISTREPLY_GAME_UNKNOWN1           0x0001 //0x0000 //0x0001 // 0x0000
#define SERVER_GAMELISTREPLY_GAME_UNKNOWN3           0x0002
#define SERVER_GAMELISTREPLY_GAME_UNKNOWN4       0x00000000
#define SERVER_GAMELISTREPLY_GAME_UNKNOWN5       0x00000000
#define SERVER_GAMELISTREPLY_GAME_STATUS_OPEN    0x00000004
#define SERVER_GAMELISTREPLY_GAME_STATUS_FULL    0x00000006
#define SERVER_GAMELISTREPLY_GAME_STATUS_STARTED 0x0000000e
#define SERVER_GAMELISTREPLY_GAME_STATUS_DONE    0x0000000c
#define SERVER_GAMELISTREPLY_GAME_UNKNOWN6       0x0000002b /* latency? */

#define SERVER_GAMELISTREPLY_TYPE_DIABLO2_OPEN 		0x0704 /* open game */
	/******************************************************/

} /* namespace pvpgn */

#endif /* INCLUDED_BNET_PROTOCOL_CHAT_GAME_H */
