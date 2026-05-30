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

/* Handshake packets: COMPINFO1/2, COMPREPLY, SESSIONKEY, COUNTRYINFO1, AUTH_INFO */
#ifndef INCLUDED_BNET_PROTOCOL_HANDSHAKE_H
#define INCLUDED_BNET_PROTOCOL_HANDSHAKE_H

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

#define CLIENT_COMPINFO1 0x05ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        reg_version;  /* 01 00 00 00 */
		bn_int        reg_auth;     /* D1 43 88 AA */ /* looks like server ip */
		bn_int        client_id;    /* DA 9D 1B 00 */
		bn_int        client_token; /* 9A F7 69 AB */
		/* host */ /* optional */
		/* user */ /* optional */
	} PACKED_ATTR() t_client_compinfo1;
#define CLIENT_COMPINFO1_REG_VERSION  0x00000001
#define CLIENT_COMPINFO1_REG_AUTH     0xaa8843d1
#define CLIENT_COMPINFO1_CLIENT_ID    0x001b9dda
#define CLIENT_COMPINFO1_CLIENT_TOKEN 0xab69f79a
	/******************************************************/


	/******************************************************/
	/*
	CLIENT_COMPINFO2 was first seen in Starcraft 1.05

	FF 1E 24 00 01 00 00 00            ..$.....
	01 00 00 00 D1 43 88 AA   1C B9 48 00 31 8A F2 89    .....C....H.1...
	43 4C 4F 55 44 00 63 6C   6F 75 64 00                CLOUD.cloud.

	FF 1E 28 00 01 00 00 00   01 00 00 00 D1 43 88 AA    ..(..........C..
	DA 9D 1B 00 9A F7 69 AB   42 4F 42 20 20 20 20 20    ......i.BOB
	20 20 20 00 42 6F 62 00                                 .Bob.

	Diablo II 1.03 ... note it sends empty host and user strings
	FF 1E 1A 00 01 00   00 00 01 00 00 00 D1 43      ............C
	88 AA DA 9D 1B 00 9A F7   69 AB 00 00                .......i...
	*/
#define CLIENT_COMPINFO2 0x1eff
	typedef struct
	{
		t_bnet_header h;
		bn_int        unknown1;     /* 01 00 00 00 */ /* ??? Version */
		bn_int        reg_version;  /* 01 00 00 00 */
		bn_int        reg_auth;     /* D1 43 88 AA */ /* looks like server ip */
		bn_int        client_id;    /* 1C B9 48 00 */ /* DA 9D 1B 00 */
		bn_int        client_token; /* 31 8A f2 89 */ /* 9A F7 69 AB */
		/* host */
		/* user */
	} PACKED_ATTR() t_client_compinfo2;
#define CLIENT_COMPINFO2_UNKNOWN1     0x00000001
#define CLIENT_COMPINFO2_REG_VERSION  0x00000001
#define CLIENT_COMPINFO2_REG_AUTH     0xaa8843d1
#define CLIENT_COMPINFO2_CLIENT_ID    0x001b9dda
#define CLIENT_COMPINFO2_CLIENT_TOKEN 0xab69f79a
	/******************************************************/


	/******************************************************/
	/*
	Sent in response to CLIENT_COMPINFO[12] along with sessionkey

	FF 05 14 00 01 00 00 00            ........
	D1 43 88 AA 1C B9 48 00   31 8A F2 89                .C....H.1...

	FF 05 14 00 01 00 00 00   D8 94 F6 07 3F 62 6E 02    ............?bn.
	CA A4 0D 99                                          ....

	FF 05 14 00 01 00 00 00   D8 94 F6 07 B3 2C 6E 02    .............,n.
	B4 E0 3B 6C                                          ..;l

	To D2 Beta 1.02:
	FF 05 14 00 01 00 00 00   D1 43 88 AA 42 8D 2E 02    .........C..B...
	2B 81 8C 2B                                          +..+
	*/
#define SERVER_COMPREPLY 0x05ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        reg_version;  /* 01 00 00 00 */
		bn_int        reg_auth;     /* D1 43 88 AA */ /* looks like server ip */
		bn_int        client_id;    /* DA 9D 1B 00 */ /* 1C B9 48 00 */
		bn_int        client_token; /* 9A F7 69 AB */ /* 31 8A F2 89 */
	} PACKED_ATTR() t_server_compreply;
#define SERVER_COMPREPLY_REG_VERSION  0x00000001
#define SERVER_COMPREPLY_REG_AUTH     0xaa8843d1
#define SERVER_COMPREPLY_CLIENT_ID    0x001b9dda
#define SERVER_COMPREPLY_CLIENT_TOKEN 0xab69f79a
	/******************************************************/


	/******************************************************/
	/*
	Sent in repsonse to COMPINFO1 along with COMPINFOREPLY.
	Used for password hashing by the client.
	*/
#define SERVER_SESSIONKEY1 0x28ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        sessionkey;
	} PACKED_ATTR() t_server_sessionkey1;
	/******************************************************/


	/******************************************************/
	/*
	Sent in response to COMPINFO2 along with COMPINFOREPLY.
	Used for password hashing by the client.

	FF 1D 0C 00 40 24 02 00            ....@$..
	99 F3 FD 78                                          ...x

	FF 1D 0C 00 0C 67 08 00   7A 3C D8 75                .....g..z<.u

	FF 1D 0C 00 58 77 00 00   27 45 44 7A                ....Xw..'EDz

	FF 1D 0C 00 9D DF 01 00   7A 11 07 ED                ........z...
	*/
#define SERVER_SESSIONKEY2 0x1dff
	typedef struct
	{
		t_bnet_header h;
		bn_int        sessionnum;
		bn_int        sessionkey;
	} PACKED_ATTR() t_server_sessionkey2;
#define SERVER_SESSIONKEY2_UNKNOWN1 0x00004df3
	/******************************************************/


	/******************************************************/
	/*
	FF 12 3C 00 E0 28 02 E4   0A 37 BE 01 E0 50 A3 37    ..<..(...7...P.7
	D0 36 BE 01 A4 01 00 00   09 04 00 00 09 04 00 00    .6..............
	09 04 00 00 65 6E 75 00   31 00 55 53 41 00 55 6E    ....enu.1.USA.Un
	69 74 65 64 20 53 74 61   74 65 73 00                ited States.

	still original client, but at a later date
	FF 12 3C 00 60 C5 4B 8B   19 DE BE 01 60 55 B1 40    ..<.`.K.....`U.@
	E7 DD BE 01 A4 01 00 00   09 04 00 00 09 04 00 00    ................
	09 04 00 00 65 6E 75 00   31 00 55 53 41 00 55 6E    ....enu.1.USA.Un
	69 74 65 64 20 53 74 61   74 65 73 00                ited States.

	FF 12 3C 00 60 EA 02 23   F5 DE BE 01 60 7A 68 D8    ..<.`..#....`zh.
	C2 DE BE 01 A4 01 00 00   09 04 00 00 09 04 00 00    ................
	09 04 00 00 65 6E 75 00   31 00 55 53 41 00 55 6E    ....enu.1.USA.Un
	69 74 65 64 20 53 74 61   74 65 73 00                ited States.

	FF 12 35 00                ..5.
	20 BA B0 55 F2 7B BE 01   20 62 98 C5 3D 7C BE 01     ..U.{.. b..=|..
	E4 FD FF FF 12 04 00 00   12 04 00 00 12 04 00 00    ................
	6B 6F 72 00 38 32 00 4B   4F 52 00 4B 6F 72 65 61    kor.82.KOR.Korea
	00                                                   .

	FF 12 37 00 E0 D4 72 97   2F 8C BF 01 E0 3C 37 F9    ..7...r./....<7.
	37 8C BF 01 C4 FF FF FF   07 04 00 00 07 04 00 00    7...............
	07 04 00 00 64 65 75 00   34 39 00 44 45 55 00 47    ....deu.49.DEU.G
	65 72 6D 61 6E 79 00                                 ermany.

	FF 12 36 00 20 F3 31 08   40 A7 BF 01 20 C3 BA CB    ..6. .1.@... ...
	50 A7 BF 01 C4 FF FF FF   1D 04 00 00 1D 04 00 00    P...............
	1D 04 00 00 73 76 65 00   34 36 00 53 57 45 00 53    ....sve.46.SWE.S
	77 65 64 65 6E 00                                    weden.

	Diablo II 1.03
	FF 12 39 00 A0 DB   AA 45 51 3F C0 01 A0 EB      ..9....EQ?....
	56 17 A5 3F C0 01 A8 FD   FF FF 09 0C 00 00 09 0C    V..?............
	00 00 09 0C 00 00 65 6E   61 00 36 31 00 41 55 53    ......ena.61.AUS
	00 41 75 73 74 72 61 6C   69 61 00                   .Australia.
	*/
#define CLIENT_COUNTRYINFO1 0x12ff
	typedef struct
	{
		t_bnet_header h;
		bn_long       systemtime; /* GMT */
		bn_long       localtime;  /* time in local timezone */
		bn_int        bias;       /* (gmt-local)/60  (using signed math) */
		bn_int        langid1;    /* 09 04 00 00 */   /* 12 04 00 00 */
		bn_int        langid2;    /* 09 04 00 00 */   /* 12 04 00 00 */
		bn_int        langid3;    /* 09 04 00 00 */   /* 12 04 00 00 */
		/* langstr */
		/* countrycode (long distance phone) */
		/* countryabbrev */
		/* countryname */
	} PACKED_ATTR() t_client_countryinfo1;
	/******************************************************/


	/******************************************************/
	/*
	  First seen in Diablo II (and LoD) 1.09
	  FF 50 34 00 00 00 00 00   36 38 58 49 50 58 32 44    .P4.....68XIPX2D
	  09 00 00 00 00 00 00 00   00 00 00 00 C4 FF FF FF    ................
	  07 04 00 00 07 04 00 00   44 45 55 00 47 65 72 6D    ........DEU.Germ
	  61 6E 79 00                                          any.

	  FF 50 47 00 00 00 00 00   36 38 58 49 56 44 32 44    .PG.....68XIVD2D
	  09 00 00 00 00 00 00 00   00 00 00 00 20 FE FF FF    ............ ...
	  04 08 00 00 04 08 00 00   43 48 4E 00 50 65 6F 70    ........CHN.Peop
	  6C 65 27 73 20 52 65 70   75 62 6C 69 63 20 6F 66    le's Republic of
	  20 43 68 69 6E 61 00                                  China.
	  */
#define CLIENT_AUTH_INFO 0x50ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        protocol;  /* 00 00 00 00 always zero */
		bn_int        archtag;
		bn_int        clienttag;
		bn_int        versionid; /* 09 00 00 00 */ /* FIXME: what is this? */
		bn_int        gamelang;  /* 00 00 00 00 always zero */
		bn_int        localip;  /* 00 00 00 00 always zero */
		bn_int        bias;      /* (gmt-local)/60  (using signed math) */
		bn_int        lcid;      /* Win32 LCID */
		bn_int        langid;    /* Win32 LangID */
		/* langstr */
		/* countryname */
	} PACKED_ATTR() t_client_auth_info;
	/******************************************************/


	/******************************************************/
	/*
	FF 2A 20 00 91 4F 93 DF   57 74 B5 C8 48 0F 4D 9B    .* ..O..Wt..H.M.
	A2 28 A6 03 C1 D9 DA 11   42 69 6D 42 6F 3A 29 00    .(......BimBo:).
	*/

} /* namespace pvpgn */

#endif /* INCLUDED_BNET_PROTOCOL_HANDSHAKE_H */
