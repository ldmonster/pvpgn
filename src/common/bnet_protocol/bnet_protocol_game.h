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

/* Game packets: STARTGAME1/3/4, CLOSEGAME, LEAVECHANNEL, MAPAUTHREQ */
#ifndef INCLUDED_BNET_PROTOCOL_GAME_H
#define INCLUDED_BNET_PROTOCOL_GAME_H

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

#define CLIENT_STARTGAME1 0x08ff /* original starcraft or shareware (1.01) */
	typedef struct
	{
		t_bnet_header h;
		bn_int        status;
		bn_int        unknown3;
		bn_short      gametype;
		bn_short      unknown1;
		bn_int        unknown4;
		bn_int        unknown5;
		/* game name */
		/* game password */
		/* game info */
	} PACKED_ATTR() t_client_startgame1;
	/* I have also seen 1,5,7,f */
#define CLIENT_STARTGAME1_STATUSMASK     0x0000000f
#define CLIENT_STARTGAME1_STATUS_OPEN    0x00000004
#define CLIENT_STARTGAME1_STATUS_FULL    0x00000006
#define CLIENT_STARTGAME1_STATUS_STARTED 0x0000000e
#define CLIENT_STARTGAME1_STATUS_DONE    0x0000000c
	/******************************************************/


	/******************************************************/
	/*
	FF 1B 14 00 02 00 17 E0   80 7B 3F 54 00 00 00 00    .........{?T....
	00 00 00 00                                          ....
	*/
#define CLIENT_UNKNOWN_1B 0x1bff
	typedef struct
	{
		t_bnet_header h;
		bn_short      unknown1; /* FIXME: This "2" is the same as in the game
					   listings. What do they mean? */
		bn_short      port;     /* big endian byte order */
		bn_int        ip;       /* big endian byte order */
		bn_int        unknown2;
		bn_int        unknown3;
	} PACKED_ATTR() t_client_unknown_1b;
#define CLIENT_UNKNOWN_1B_UNKNOWN1 0x0002
#define CLIENT_UNKNOWN_1B_UNKNOWN2 0x00000000
#define CLIENT_UNKNOWN_1B_UNKNOWN3 0x00000000
	/******************************************************/


	/******************************************************/
	/*
	FF 1A 4C 00 01 00 00 00   00 00 00 00 00 00 00 00    ..L.............
	0F 00 00 00 00 00 00 00   E0 17 00 00 61 6E 73 00    ............ans.
	65 6C 6D 6F 00 30 0D 77   61 72 72 69 6F 72 0D 4C    elmo.0.warrior.L
	54 52 44 20 31 20 30 20   30 20 33 30 20 31 30 20    TRD 1 0 0 30 10
	32 30 20 32 35 20 31 30   30 20 30 00                20 25 100 0.
	*/
#define CLIENT_STARTGAME3 0x1aff /* Starcraft 1.03, Diablo 1.07 */
	typedef struct
	{
		t_bnet_header h;
		bn_int        status;
		bn_int        unknown3;
		bn_short      gametype;
		bn_short      unknown1;
		bn_int        unknown6;
		bn_int        unknown4; /* port # under Diablo */
		bn_int        unknown5;
		/* game name */
		/* game password */
		/* game info */
	} PACKED_ATTR() t_client_startgame3;
#define CLIENT_STARTGAME3_STATUSMASK      0x0000000f
#define CLIENT_STARTGAME3_STATUS_OPEN1    0x00000001 /* used by Diablo */
#define CLIENT_STARTGAME3_STATUS_OPEN     0x00000004
#define CLIENT_STARTGAME3_STATUS_FULL     0x00000006
#define CLIENT_STARTGAME3_STATUS_STARTED  0x0000000e
#define CLIENT_STARTGAME3_STATUS_DONE     0x0000000c
	/******************************************************/


	/******************************************************/
	/*
	FF 1C 49 00 00 00 00 00   00 00 00 00 03 00 01 00    ..I.............
	00 00 00 00 00 00 00 00   74 65 61 6D 6D 65 6C 65    ........teammele
	65 00 00 2C 2C 2C 2C 31   2C 33 2C 31 2C 33 65 33    e..,,,,1,3,1,3e3
	37 61 38 34 63 2C 37 2C   61 6E 73 65 6C 6D 6F 0D    7a84c,7,anselmo.
	4F 63 74 6F 70 75 73 0D   00                         Octopus..

	Brood War 1.04 ladder, disconnect==loss
	FF 1C 45 00 10 00 00 00   00 00 00 00 09 00 02 00    ..E.............
	00 00 00 00 01 00 00 00   54 45 53 54 00 00 2C 34    ........TEST..,4
	34 2C 31 34 2C 2C 32 2C   39 2C 32 2C 33 65 33 37    4,14,,2,9,2,3e37
	61 38 34 63 2C 33 2C 52   6F 73 73 0D 41 73 68 72    a84c,3,Ross.Ashr
	69 67 6F 0D 00                                       igo..

	Brood War 1.04 ladder, disconnect==disconnect
	FF 1C 45 00 00 00 00 00   00 00 00 00 09 00 01 00    ..E.............
	00 00 00 00 01 00 00 00   54 45 53 54 00 00 2C 34    ........TEST..,4
	34 2C 31 34 2C 2C 32 2C   39 2C 31 2C 33 65 33 37    4,14,,2,9,1,3e37
	61 38 34 63 2C 33 2C 52   6F 73 73 0D 41 73 68 72    a84c,3,Ross.Ashr
	69 67 6F 0D 00                                       igo..

	Brood War 1.04 greed, minerals==10000
	FF 1C 4C 00 00 00 00 00   00 00 00 00 06 00 04 00    ..L.............
	00 00 00 00 00 00 00 00   74 65 73 74 31 30 30 30    ........test1000
	30 00 00 2C 33 34 2C 31   32 2C 2C 31 2C 36 2C 34    0..,34,12,,1,6,4
	2C 33 65 33 37 61 38 34   63 2C 2C 52 6F 73 73 0D    ,3e37a84c,,Ross.
	43 68 61 6C 6C 65 6E 67   65 72 0D 00                Challenger..

	FF 1C 4F 00                                          ..O.
	00 00 00 00 00 00 00 00   02 00 01 00 1F 00 00 00    ................
	00 00 00 00 74 65 73 74   00 00 2C 34 34 2C 31 34    ....test..,44,14
	2C 35 2C 32 2C 32 2C 31   2C 32 31 30 34 62 62 33    ,5,2,2,1,2104bb3
	36 2C 34 2C 48 6F 6D 65   72 0D 54 68 65 20 4C 6F    6,4,Homer.The.Lo
	73 74 20 54 65 6D 70 6C   65 0D 00                   st.Temple..

	Diablo II 1.03 (level diff 0)
	FF 1C 20 00 00 00   00 00 00 00 00 00 00 00      .. ... ........
	00 00 00 00 00 00 00 00   00 00 54 65 73 74 00 00    ........ ..Test..
	31 00                                                1.
	*/
#define CLIENT_STARTGAME4 0x1cff /* Brood War or newer Starcraft (1.04, 1.05) */
	typedef struct
	{
		t_bnet_header h;
		bn_short      status; /* 0x0001 - private war3 game */
		bn_short      flag;
		bn_int        unknown2; /* 00 00 00 00 */
		bn_short      gametype;
		bn_short      option;   /* 01 00 */
		bn_int        unknown4; /* 00 00 00 00 */
		bn_int        unknown5; /* 00 00 00 00 */
		/* game name */
		/* game password */
		/* game info */
	} PACKED_ATTR() t_client_startgame4;
#define CLIENT_STARTGAME4_UNKNOWN2		    0x00000000
#define CLIENT_STARTGAME4_STATUSMASK_16             0x000000ff
#define CLIENT_STARTGAME4_STATUSMASK_INIT_VALID     0x00000093
#define CLIENT_STARTGAME4_STATUSMASK_OPEN_VALID     0x0000009f
#define CLIENT_STARTGAME4_STATUS_INIT               0x00000000
#define CLIENT_STARTGAME4_STATUS_PRIVATE            0x00000001
#define CLIENT_STARTGAME4_STATUS_FULL               0x00000002
#define CLIENT_STARTGAME4_STATUS_OPEN               0x00000004
#define CLIENT_STARTGAME4_STATUS_START              0x00000008
#define CLIENT_STARTGAME4_STATUS_DISC_IS_LOSS       0x00000010
#define CLIENT_STARTGAME4_STATUS_REPLAY             0x00000080
#define CLIENT_STARTGAME4_OPTION_MELEE_NORMAL       0x0001
#define CLIENT_STARTGAME4_OPTION_FFA_NORMAL         0x0001
#define CLIENT_STARTGAME4_OPTION_ONEONONE_NORMAL    0x0001
#define CLIENT_STARTGAME4_OPTION_CTF_NORMAL         0x0001
#define CLIENT_STARTGAME4_OPTION_GREED_10000        0x0004
#define CLIENT_STARTGAME4_OPTION_GREED_7500         0x0003
#define CLIENT_STARTGAME4_OPTION_GREED_5000         0x0002
#define CLIENT_STARTGAME4_OPTION_GREED_2500         0x0001
#define CLIENT_STARTGAME4_OPTION_SLAUGHTER_60       0x0004
#define CLIENT_STARTGAME4_OPTION_SLAUGHTER_45       0x0003
#define CLIENT_STARTGAME4_OPTION_SLAUGHTER_30       0x0002
#define CLIENT_STARTGAME4_OPTION_SLAUGHTER_15       0x0001
#define CLIENT_STARTGAME4_OPTION_SDEATH_NORMAL      0x0001
#define CLIENT_STARTGAME4_OPTION_LADDER_COUNTASLOSS 0x0002
#define CLIENT_STARTGAME4_OPTION_LADDER_NOPENALTY   0x0001
#define CLIENT_STARTGAME4_OPTION_IRONMAN_           0x000 /* FIXME */
#define CLIENT_STARTGAME4_OPTION_MAPSET_NORMAL      0x0001
#define CLIENT_STARTGAME4_OPTION_TEAMMELEE_4        0x0003
#define CLIENT_STARTGAME4_OPTION_TEAMMELEE_3        0x0002
#define CLIENT_STARTGAME4_OPTION_TEAMMELEE_2        0x0001
#define CLIENT_STARTGAME4_OPTION_TEAMFFA_4          0x0003
#define CLIENT_STARTGAME4_OPTION_TEAMFFA_3          0x0002
#define CLIENT_STARTGAME4_OPTION_TEAMFFA_2          0x0001
#define CLIENT_STARTGAME4_OPTION_TEAMCTF_4          0x0003
#define CLIENT_STARTGAME4_OPTION_TEAMCTF_3          0x0002
#define CLIENT_STARTGAME4_OPTION_TEAMCTF_2          0x0001
#define CLIENT_STARTGAME4_OPTION_PGL_               0x000 /* FIXME */
#define CLIENT_STARTGAME4_OPTION_TOPVBOT_1          0x0001 /* 1 vs all [ 1x1, 1x2, 1x3 ...]         */
#define CLIENT_STARTGAME4_OPTION_TOPVBOT_2          0x0002 /* f.e. for (8) The Hunters.scm 1 vs all */
#define CLIENT_STARTGAME4_OPTION_TOPVBOT_3          0x0003 /*      means 1x7                        */
#define CLIENT_STARTGAME4_OPTION_TOPVBOT_4          0x0004 /* 4 vs all                              */
#define CLIENT_STARTGAME4_OPTION_TOPVBOT_5          0x0005
#define CLIENT_STARTGAME4_OPTION_TOPVBOT_6          0x0006
#define CLIENT_STARTGAME4_OPTION_TOPVBOT_7          0x0007
#define CLIENT_STARTGAME4_UNKNOWN4		    0x00000000
#define CLIENT_STARTGAME4_UNKNOWN5		    0x00000000
#define CLIENT_STARTGAME4_OPTION_NONE               0x000 /* FIXME */

#define CLIENT_STARTGAME4_FLAG_PRIVATE              0x0001
#define CLIENT_STARTGAME4_FLAG_PRIVATE_PASSWORD     "password"

#define CLIENT_MAPTYPE_SELFMADE     0
#define CLIENT_MAPTYPE_BLIZZARD     1
#define CLIENT_MAPTYPE_LADDER       2
#define CLIENT_MAPTYPE_PGL          3
#define CLIENT_MAPTYPE_KBK          4
#define CLIENT_MAPTYPE_CompUSA      5

	/* CLIENT_GAMESPEED_FAST is NULL for "fast" games, I convert it into 4 */
#define CLIENT_GAMESPEED_SLOWEST  0
#define CLIENT_GAMESPEED_SLOWER   1
#define CLIENT_GAMESPEED_SLOW     2
#define CLIENT_GAMESPEED_NORMAL   3
#define CLIENT_GAMESPEED_FAST     4
#define CLIENT_GAMESPEED_FASTER   5
#define CLIENT_GAMESPEED_FASTEST  6

	/* The tileset is NULL for BADLANDS, I'm using zero here */
#define CLIENT_TILESET_BADLANDS       0
#define CLIENT_TILESET_SPACE          1
#define CLIENT_TILESET_INSTALLATION   2
#define CLIENT_TILESET_ASHWORLD       3
#define CLIENT_TILESET_JUNGLE         4
#define CLIENT_TILESET_DESERT         5
#define CLIENT_TILESET_ICE            6
#define CLIENT_TILESET_TWILIGHT       7

	/* Diablo II Difficulty */
#define CLIENT_DIFFICULTY_NORMAL             1
#define CLIENT_DIFFICULTY_NIGHTMARE          2
#define CLIENT_DIFFICULTY_HELL               3 /* assumed */
#define CLIENT_DIFFICULTY_HARDCORE_NORMAL    4
#define CLIENT_DIFFICULTY_HARDCORE_NIGHTMARE 5 /* assumed */
#define CLIENT_DIFFICULTY_HARDCORE_HELL      6 /* assumed */
	/******************************************************/


	/******************************************************/
#define SERVER_STARTGAME1_ACK 0x08ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        reply;
	} PACKED_ATTR() t_server_startgame1_ack;
#define SERVER_STARTGAME1_ACK_NO 0x00000000
#define SERVER_STARTGAME1_ACK_OK 0x00000001
	/******************************************************/


	/******************************************************/
#define SERVER_STARTGAME3_ACK 0x1aff
	typedef struct
	{
		t_bnet_header h;
		bn_int        reply;
	} PACKED_ATTR() t_server_startgame3_ack;
#define SERVER_STARTGAME3_ACK_NO 0x00000000
#define SERVER_STARTGAME3_ACK_OK 0x00000001
	/******************************************************/


	/******************************************************/
#define SERVER_STARTGAME4_ACK 0x1cff
	typedef struct
	{
		t_bnet_header h;
		bn_int        reply;
	} PACKED_ATTR() t_server_startgame4_ack;
#define SERVER_STARTGAME4_ACK_NO 0x00000001
#define SERVER_STARTGAME4_ACK_OK 0x00000000
	/******************************************************/


	/******************************************************/
#define CLIENT_CLOSEGAME 0x02ff
#define CLIENT_CLOSEGAME2 0x1fff
	typedef struct
	{
		t_bnet_header h;
	} PACKED_ATTR() t_client_closegame;
	/******************************************************/


	/******************************************************/
#define CLIENT_LEAVECHANNEL 0x10ff
	typedef struct
	{
		t_bnet_header h;
	} PACKED_ATTR() t_client_leavechannel;

	/******************************************************/
	/*
		packets 0x60ff - 0x64ff moved to anongame_protocal.h
		[Omega]
		*/
	/******************************************************/

	/*
	FF 32 2A 00 1A 29 25 72   77 C3 3C 25 6B 4D 7A A4    .2*..)%rw.<%kMz.
	3B 92 38 D5 01 F4 A5 6B   28 32 29 43 68 61 6C 6C    ;.8....k(2)Chall
	65 6E 67 65 72 2E 73 63   6D 00                      enger.scm.

	FF 32 2C 00 21 F8 16 2D   99 D9 BC A4 A6 5C BA 60    .2,.!..-.....\.`
	71 DE 6D 64 6F BC A5 03   28 34 29 44 69 72 65 20    q.mdo...(4)Dire
	53 74 72 61 69 74 73 2E   73 63 6D 00                Straits.scm.
	*/
#define CLIENT_MAPAUTHREQ1 0x32ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        file_checksum[5];
		/* mapfile */
	} PACKED_ATTR() t_client_mapauthreq1;
	/******************************************************/


	/******************************************************/
	/*
	FF 32 08 00 01 00 00 00                              .2......
	*/
#define SERVER_MAPAUTHREPLY1 0x32ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        response;
	} PACKED_ATTR() t_server_mapauthreply1;
#define SERVER_MAPAUTHREPLY1_NO        0x00000000
#define SERVER_MAPAUTHREPLY1_OK        0x00000001
#define SERVER_MAPAUTHREPLY1_LADDER_OK 0x00000002
	/******************************************************/


	/******************************************************/
	/*
	From BW1.08alpha:
	FF 3C 31 00 7A 20 01 00   3B B7 C6 27 0D 61 C3 79    .<1.z ..;..'.a.y
	79 BE 24 5E 9C 07 05 7D   0B 6A A0 78 28 35 29 4A    y.$^...}.j.x(5)J
	65 77 65 6C 65 64 20 52   69 76 65 72 2E 73 63 6D    eweled River.scm
	00                                                   .
	*/
#define CLIENT_MAPAUTHREQ2 0x3cff
	typedef struct
	{
		t_bnet_header h;
		bn_int        unknown;
		bn_int        file_hash[5];
		/* mapfile */
	} PACKED_ATTR() t_client_mapauthreq2;
	/******************************************************/


	/******************************************************/
	/*
	assuming it looks like the REPLY1 packet....
	*/
#define SERVER_MAPAUTHREPLY2 0x3cff
	typedef struct
	{
		t_bnet_header h;
		bn_int        response;
	} PACKED_ATTR() t_server_mapauthreply2;
#define SERVER_MAPAUTHREPLY2_NO        0x00000000 /* FIXME: these values are guesses */
#define SERVER_MAPAUTHREPLY2_OK        0x00000001
#define SERVER_MAPAUTHREPLY2_LADDER_OK 0x00000002
	/******************************************************/


	/******************************************************/
	/*
	FF 15 14 00 36 38 58 49 52 41 54 53 00 00 00 00  ....68XIRATS....
	AF 14 55 36                                      ..U6
	*/

} /* namespace pvpgn */

#endif /* INCLUDED_BNET_PROTOCOL_GAME_H */
