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

/* Auth packets: CREATEACCT, LOGINREQ, LOGONPROOF, PASSCHANGE, CDKEY, etc. */
#ifndef INCLUDED_BNET_PROTOCOL_AUTH_H
#define INCLUDED_BNET_PROTOCOL_AUTH_H

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

#define CLIENT_CREATEACCTREQ1 0x2aff
	typedef struct
	{
		t_bnet_header h;
		bn_int        password_hash1[5]; /* hash of lowercase password w/o null */
		/* player name */
	} PACKED_ATTR() t_client_createacctreq1;
	/******************************************************/


	/******************************************************/
	/*
							  FF 2A 18 00 01 00 00 00            .*......
							  13 00 00 00 78 52 82 02   00 00 00 00 00 00 00 00    ................
							  ---120 82 130 2---

							  FF 2A 08 00 01 00 00 00                              .*......
							  */
#define SERVER_CREATEACCTREPLY1 0x2aff
	typedef struct
	{
		t_bnet_header h;
		bn_int        result;
	} PACKED_ATTR() t_server_createacctreply1;
#define SERVER_CREATEACCTREPLY1_RESULT_OK 0x00000001
#define SERVER_CREATEACCTREPLY1_RESULT_NO 0x00000000
	/******************************************************/


	/******************************************************/
	/*
							  FF 2B 20 00 01 00 00 00            .+ .....
							  00 00 00 00 4D 00 00 00   0E 01 00 00 20 00 00 00    ....M....... ...
							  CE 01 00 00 DD 07 00 00                              ........

							  FF 2B 20 00 01 00 00   00 00 00 00 00 06 00 00     .+ ............
							  00 72 01 00 00 40 00 00   00 A9 07 00 00 FF 07 00    .r...@..........
							  00                                                   .

							  from Starcraft 1.05
							  FF 2B 20 00 01 00 00 00   00 00 00 00 06 00 00 00    .+ .............
							  7C 01 00 00 20 00 00 00   00 02 00 00 FF 07 00 00    |... ...........
							  */
#define CLIENT_UNKNOWN_2B 0x2bff /* FIXME: what is this? */
	typedef struct
	{
		t_bnet_header h;
		bn_int        unknown1; /* 01 00 00 00 */ /* 01 00 00 00 */
		bn_int        unknown2; /* 00 00 00 00 */ /* 00 00 00 00 */
		bn_int        unknown3; /* 4D 00 00 00 */ /* 06 00 00 00 */
		bn_int        unknown4; /* 0E 01 00 00 */ /* 72 01 00 00 */
		bn_int        unknown5; /* 20 00 00 00 */ /* 40 00 00 00 */
		bn_int        unknown6; /* CE 01 00 00 */ /* A9 07 00 00 */
		bn_int        unknown7; /* DD 07 00 00 */ /* FF 07 00 00 */
	} PACKED_ATTR() t_client_unknown_2b;
#define CLIENT_UNKNOWN_2B_UNKNOWN1 0x00000001
#define CLIENT_UNKNOWN_2B_UNKNOWN2 0x00000000
#define CLIENT_UNKNOWN_2B_UNKNOWN3 0x0000004d
#define CLIENT_UNKNOWN_2B_UNKNOWN4 0x0000010e
#define CLIENT_UNKNOWN_2B_UNKNOWN5 0x00000020
#define CLIENT_UNKNOWN_2B_UNKNOWN6 0x000001ce
#define CLIENT_UNKNOWN_2B_UNKNOWN7 0x000007dd
	/******************************************************/


	/******************************************************/
	/*
	later replaced by progident2 and the authreq packets

	FF 06 14 00 36 38 58 49            ....68XI
	50 58 45 53 BB 00 00 00   00 00 00 00                PXES........

	sent by 1.05 Starcraft
	FF 06 14 00 36 38 58 49   52 41 54 53 BD 00 00 00    ....68XIRATS....
	00 00 00 00                                          ....

	Diablo II 1.03
	FF 06 14 00 36 38   58 49 56 44 32 44 03 00      ....68XIVD2D..
	00 00 00 00 00 00                                    ......
	*/
#define CLIENT_PROGIDENT 0x06ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        archtag;
		bn_int        clienttag; /* see tag.h */
		bn_int        versionid; /* FIXME: how does the versionid work? */
		bn_int        unknown1;  /* FIXME: always zero? spawn flag? */
	} PACKED_ATTR() t_client_progident;
	/******************************************************/


	/******************************************************/
	/*
	These formulas are for authenticating the client version.

	FF 06 5A 00 00 86 BA E3            ..Z.....
	09 28 BC 01 49 58 38 36   76 65 72 32 2E 6D 70 71    .(..IX86ver2.mpq
	00 41 3D 32 30 31 39 34   39 38 38 39 39 20 42 3D    .A=2019498899 B=
	33 34 32 33 32 39 32 33   39 34 20 43 3D 31 37 31    3423292394 C=171
	39 30 31 31 32 32 32 20   34 20 41 3D 41 5E 53 20    9011222 4 A=A^S
	42 3D 42 2D 43 20 43 3D   43 5E 41 20 41 3D 41 5E    B=B-C C=C^A A=A^
	42 00                                                B.

	FF 06 59 00 00 C1 12 EC   09 28 BC 01 49 58 38 36    ..Y......(..IX86
	76 65 72 35 2E 6D 70 71   00 41 3D 31 38 37 35 35    ver5.mpq.A=18755
	39 31 33 34 31 20 42 3D   32 34 39 31 30 39 39 38    91341 B=24910998
	30 39 20 43 3D 36 33 34   38 35 36 36 30 34 20 34    09 C=634856604 4
	20 41 3D 41 2D 53 20 42   3D 42 5E 43 20 43 3D 43     A=A-S B=B^C C=C
	2B 41 20 41 3D 41 5E 42   00                         +A A=A^B.

	FF 06 5A 00 00 C1 12 EC   09 28 BC 01 49 58 38 36    ..Z......(..IX86
	76 65 72 35 2E 6D 70 71   00 41 3D 31 37 31 32 39    ver5.mpq.A=17129
	34 38 34 32 36 20 42 3D   33 36 30 30 30 33 30 36    48426 B=36000306
	30 37 20 43 3D 33 33 39   30 34 31 37 39 35 39 20    07 C=3390417959
	34 20 41 3D 41 2D 53 20   42 3D 42 5E 43 20 43 3D    4 A=A-S B=B^C C=
	43 2D 41 20 41 3D 41 2D   42 00                      C-A A=A-B.

	FF 06 5A 00 00 3A 7F E8   09 28 BC 01 49 58 38 36    ..Z..:...(..IX86
	76 65 72 34 2E 6D 70 71   00 41 3D 31 31 38 36 39    ver4.mpq.A=11869
	35 38 31 34 31 20 42 3D   31 33 37 37 34 34 31 34    58141 B=13774414
	35 37 20 43 3D 31 37 37   32 37 38 37 37 30 35 20    57 C=1772787705
	34 20 41 3D 41 5E 53 20   42 3D 42 5E 43 20 43 3D    4 A=A^S B=B^C C=
	43 2B 41 20 41 3D 41 5E   42 00                      C+A A=A^B.

	FF 06 5A 00 00 56 CD F6   09 28 BC 01 49 58 38 36    ..Z..V...(..IX86
	76 65 72 37 2E 6D 70 71   00 41 3D 31 30 32 36 30    ver7.mpq.A=10260
	34 34 33 35 34 20 42 3D   34 31 33 32 36 33 30 37    44354 B=41326307
	31 31 20 43 3D 32 33 30   32 34 31 31 33 32 38 20    11 C=2302411328
	34 20 41 3D 41 5E 53 20   42 3D 42 5E 43 20 43 3D    4 A=A^S B=B^C C=
	43 5E 41 20 41 3D 41 2B   42 00                      C^A A=A+B.
	*/
#define SERVER_AUTHREQ1 0x06ff
	typedef struct
	{
		t_bnet_header h;
		bn_long       timestamp; /* FIXME: file modification time? */
		/* versioncheck filename */
		/* equation */
	} PACKED_ATTR() t_server_authreq1;
	/******************************************************/


	/******************************************************/
	/*
	  First seen in Diablo II (and LoD) 1.09
	  FF 50 65 00 00 00 00 00   36 1A 6C 45 76 BC 00 00    .Pe.....6.lEv...
	  00 48 A6 EF 09 28 BC 01   49 58 38 36 76 65 72 36    .H...(..IX86ver6
	  2E 6D 70 71 00 41 3D 33   38 34 35 35 38 31 36 33    .mpq.A=384558163
	  34 20 42 3D 38 38 30 38   32 33 35 38 30 20 43 3D    4 B=880823580 C=
	  31 33 36 33 39 33 37 31   30 33 20 34 20 41 3D 41    1363937103 4 A=A
	  2D 53 20 42 3D 42 2D 43   20 43 3D 43 2D 41 20 41    -S B=B-C C=C-A A
	  3D 41 2D 42 00                                       =A-B.

	  FF 50 65 00 00 00 00 00            .Pe.....
	  30 4B C1 33 10 EB 09 00   00 A5 C4 DD 09 28 BC 01    0K.3.........(..
	  49 58 38 36 76 65 72 30   2E 6D 70 71 00 41 3D 31    IX86ver0.mpq.A=1
	  34 33 32 36 36 32 34 37   38 20 42 3D 36 35 32 32    432662478 B=6522
	  37 38 36 32 35 20 43 3D   31 37 36 31 35 31 35 38    78625 C=17615158
	  36 39 20 34 20 41 3D 41   5E 53 20 42 3D 42 2B 43    69 4 A=A^S B=B+C
	  20 43 3D 43 2B 41 20 41   3D 41 5E 42 00              C=C+A A=A^B.
	  */
#define SERVER_AUTHREQ_109 0x50ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        logontype;  /* 00 00 00 00 always zero */
		bn_int        sessionkey;
		bn_int        sessionnum;
		bn_long       timestamp;
		/* versioncheck filename */
		/* equation */
	} PACKED_ATTR() t_server_authreq_109;
#define SERVER_AUTHREQ_109_LOGONTYPE 		0x0000000
#define SERVER_AUTHREQ_109_LOGONTYPE_W3 	0x00000002
#define SERVER_AUTHREQ_109_LOGONTYPE_W3XP 	0x00000002
	/******************************************************/

	/* ADDED BY UNDYING SOULZZ 4/3/02 */
#define VERSIONTAG_WARCRAFT3_113	"WAR3_113"

	/******************************************************/
	/*
	FF 07 40 00 36 38 58 49   52 41 54 53 BD 00 00 00    ..@.68XIRATS....
	00 05 00 01 1E 88 D7 08   73 74 61 72 63 72 61 66    ........starcraf
	74 2E 65 78 65 20 30 33   2F 30 38 2F 39 39 20 32    t.exe 03/08/99 2
	32 3A 34 31 3A 35 30 20   31 30 34 32 34 33 32 00    2:41:50 1042432.

	sent by the 1.05 Starcraft
	FF 07 40 00 36 38 58 49   52 41 54 53 BD 00 00 00    ..@.68XIRATS....
	00 05 00 01 AE AC DE 87   73 74 61 72 63 72 61 66    ........starcraf
	74 2E 65 78 65 20 30 33   2F 30 38 2F 39 39 20 32    t.exe 03/08/99 2
	32 3A 34 31 3A 35 30 20   31 30 34 32 34 33 32 00    2:41:50 1042432.

	sent by the 1.08alpha Brood War (Starcraft game) in response to
	A=2521522835 B=3428392135 C=218673704 4 A=A^S B=B-C C=C+A A=A-B
	with IX86ver1.mpq
	FF 07 40 00 36 38 58 49   52 41 54 53 C3 00 00 00    ..@.68XIRATS....
	01 08 00 01 A1 52 CE FE   73 74 61 72 63 72 61 66    .....R..starcraf
	74 2E 65 78 65 20 31 32   2F 32 38 2F 30 30 20 31    t.exe 12/28/00 1
	31 3A 32 38 3A 35 32 20   31 30 38 32 33 36 38 00    1:28:52 1082368.

	sent by the 1.07 Diablo
	FF 07 3C 00 36 38 58 49   4C 54 52 44 26 00 00 00    ..<.68XILTRD&...
	01 06 05 62 8C 56 E6 21   64 69 61 62 6C 6F 2E 65    ...b.V.!diablo.e
	78 65 20 30 39 2F 31 37   2F 39 38 20 31 38 3A 30    xe 09/17/98 18:0
	30 3A 34 30 20 37 36 30   33 32 30 00                0:40 760320.

	FF 07 45 00 36 38 58 49   4E 42 32 57 4B 00 00 00    ..E.68XINB2WK...
	99 00 00 02 3D 51 C4 AA   57 61 72 63 72 61 66 74    ....=Q..Warcraft
	20 49 49 20 42 4E 45 2E   65 78 65 20 31 30 2F 31     II BNE.exe 10/1
	35 2F 39 39 20 30 30 3A   33 37 3A 35 34 20 37 30    5/99 00:37:54 70
	34 35 31 32 00                                       4512.

	sent by the 1.03 Diablo II
	FF 07 3A 00 36 38 58 49            ..:.68XI
	56 44 32 44 03 00 00 00   00 03 00 01 47 3E 26 73    VD2D........G>&s
	47 61 6D 65 2E 65 78 65   20 30 38 2F 30 35 2F 30    Game.exe 08/05/0
	30 20 30 31 3A 34 32 3A   32 38 20 32 39 34 39 31    0 01:42:28 29491
	32 00                                                2.
	*/
#define CLIENT_AUTHREQ1 0x07ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        archtag;
		bn_int        clienttag;
		bn_int        versionid;
		bn_int        gameversion;
		bn_int        checksum;
		/* executable info */
	} PACKED_ATTR() t_client_authreq1;

	/******************************************************/


	/******************************************************/
	/*
							  FF 07 0A 00 02 00 00 00            ........
							  00 00                                                ..

							  FF 07 0A 00 02 00 00 00   00 00                      ..........
							  */
#define SERVER_AUTHREPLY1 0x07ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        message;
		/* filename */
		/* unknown */
	} PACKED_ATTR() t_server_authreply1;
#define SERVER_AUTHREPLY1_MESSAGE_BADVERSION  0x00000000
#define SERVER_AUTHREPLY1_MESSAGE_UPDATE      0x00000001 /* initiate auto-update */
#define SERVER_AUTHREPLY1_MESSAGE_OK          0x00000002
	/******************************************************/


	/******************************************************/
	/*
	  First seen in Diablo II (and LoD) 1.09
	  FF 51 09 00 00 00 00 00   00                         .Q.......
	  */
#define SERVER_AUTHREPLY_109 0x51ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        message;
		/* message string? */
	} PACKED_ATTR() t_server_authreply_109;
#define SERVER_AUTHREPLY_109_MESSAGE_OK         0x00000000
#define SERVER_AUTHREPLY_109_MESSAGE_UPDATE     0x00000100
#define SERVER_AUTHREPLY_109_MESSAGE_BADVERSION 0x00000101
	/* we should check the first 10 values or so to see what they mean */
	/******************************************************/


	/******************************************************/
	/*
	  First seen in Diablo II (and LoD) 1.09
	  FF 51 67 00 C9 88 DA 42   00 09 00 01 46 97 62 9A    .Qg....B....F.b.
	  01 00 00 00 00 00 00 00   10 00 00 00 06 00 00 00    ................
	  A5 E7 39 00 00 00 00 00   ED CD 4F F7 6A 7A 4F 96    ..9.......O.jzO.
	  85 7A 2D A2 7F 1F B1 D6   81 B3 8D 50 47 61 6D 65    .z-........PGame
	  2E 65 78 65 20 30 38 2F   31 36 2F 30 31 20 32 33    .exe 08/16/01 23
	  3A 30 34 3A 34 30 20 34   32 34 30 36 37 00 74 73    :04:40 424067.ts
	  69 6E 67 68 75 61 00                                 inghua.
	  */
#define CLIENT_AUTHREQ_109 0x51ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        ticks;
		bn_int        gameversion;
		bn_int        checksum;
		bn_int        cdkey_number; /* count of cdkeys, d2 = 1, lod = 2 */
		bn_int        spawn; 	/* set if using spawn copy */
		/* cdkey info(s) */
		/* executable info */
		/* cdkey owner */
	} PACKED_ATTR() t_client_authreq_109;
	/* values are the same as in CLIENT_AUTHREQ1 */

	typedef struct
	{
		bn_int len;
		bn_int type;
		bn_int checksum;
		bn_int u1;
		bn_int hash[5];
	} PACKED_ATTR() t_cdkey_info;
	/******************************************************/


	/******************************************************/
	/* Batle.net used to send requests for registry and email info.
	   Thanks to bnetanon, I have a dump of these old packets.
	   */
	/*
	FF 18 41 00 00 00 00 00   01 00 00 80 53 6F 66 74    ..A.........Soft
	77 61 72 65 5C 4D 69 63   72 6F 73 6F 66 74 5C 4D    ware\Microsoft\M
	53 20 53 65 74 75 70 20   28 41 43 4D 45 29 5C 55    S Setup (ACME)\U
	73 65 72 20 49 6E 66 6F   00 44 65 66 4E 61 6D 65    ser Info.DefName
	00                                                   .

	FF 18 48 00 00 00 00 00   01 00 00 80 53 6F 66 74    ..H.........Soft
	77 61 72 65 5C 4D 69 63   72 6F 73 6F 66 74 5C 4D    ware\Microsoft\M
	65 64 69 61 50 6C 61 79   65 72 5C 43 6F 6E 74 72    ediaPlayer\Contr
	6F 6C 5C 50 6C 61 79 42   61 72 00 43 6C 72 42 61    ol\PlayBar.ClrBa
	63 6B 43 6F 6C 6F 72 00                              ckColor.
	*/
#define SERVER_REGSNOOPREQ 0x18ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        unknown1; /* 00 00 00 00 */ /* sequence match like in other packets? */
		bn_int        hkey;
		/* registry key */
		/* value name */
	} PACKED_ATTR() t_server_regsnoopreq;
#define SERVER_REGSNOOPREQ_UNKNOWN1 0x00000000
#define SERVER_REGSNOOPREQ_HKEY_CLASSES_ROOT        0x80000000
#define SERVER_REGSNOOPREQ_HKEY_CURRENT_USER        0x80000001
#define SERVER_REGSNOOPREQ_HKEY_LOCAL_MACHINE       0x80000002
#define SERVER_REGSNOOPREQ_HKEY_USERS               0x80000003
#define SERVER_REGSNOOPREQ_HKEY_PERFORMANCE_DATA    0x80000004
#define SERVER_REGSNOOPREQ_HKEY_CURRENT_CONFIG      0x80000005
#define SERVER_REGSNOOPREQ_HKEY_DYN_DATA            0x80000006
#define SERVER_REGSNOOPREQ_HKEY_PERFORMANCE_TEXT    0x80000050
#define SERVER_REGSNOOPREQ_HKEY_PERFORMANCE_NLSTEXT 0x80000060
#define SERVER_REGSNOOPREQ_REGKEY "Software\\Microsoft\\MS Setup (ACME)\\User Info"
#define SERVER_REGSNOOPREQ_REGVALNAME "DefName"
	/******************************************************/


	/******************************************************/
	/* If the key exists, the client send this back */
	/*
	FF 18 0C 00 00 00 00 00   42 6F 62 00                ........Bob.

	FF 18 0C 00 00 00 00 00   A0 9C A0 00                ............
	*/
#define CLIENT_REGSNOOPREPLY 0x18ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        unknown1; /* 00 00 00 00 */ /* same as request? */
		/* registry value (string, dword, or binary */
	} PACKED_ATTR() t_client_regsnoopreply;
	/******************************************************/


	/******************************************************/
	/*
	FF 07 0A 00 02 00 00 00   00 00                      ..........
	*/
#define CLIENT_ICONREQ 0x2dff
	typedef struct
	{
		t_bnet_header h;
	} PACKED_ATTR() t_client_iconreq;
	/******************************************************/


	/******************************************************/
	/*
							  FF 2D 16 00 76 34 1F 8F            .-..v4..
							  C0 D6 BD 01 69 63 6F 6E   73 2E 62 6E 69 00          ....icons.bni.

							  FF 2D 16 00 00 77 D0 01   C7 B1 BE 01 69 63 6F 6E    .-...w......icon
							  73 2E 62 6E 69 00                                    s.bni.
							  */
#define SERVER_ICONREPLY 0x2dff
	typedef struct
	{
		t_bnet_header h;
		bn_long       timestamp; /* file modification time? */
		/* filename */
	} PACKED_ATTR() t_server_iconreply;
	/******************************************************/


	/******************************************************/
#define CLIENT_LADDERSEARCHREQ 0x2fff
	typedef struct
	{
		t_bnet_header h;
		bn_int        clienttag;
		bn_int        id;   /* (AKA ladder type) 1==standard, 3==ironman */
		bn_int        type; /* (AKA ladder sort) */
		/* player name */
	} PACKED_ATTR() t_client_laddersearchreq;
#define CLIENT_LADDERSEARCHREQ_ID_STANDARD       0x00000001
#define CLIENT_LADDERSEARCHREQ_ID_IRONMAN        0x00000003
#define CLIENT_LADDERSEARCHREQ_TYPE_HIGHESTRATED 0x00000000
#define CLIENT_LADDERSEARCHREQ_TYPE_MOSTWINS     0x00000002
#define CLIENT_LADDERSEARCHREQ_TYPE_MOSTGAMES    0x00000003
	/******************************************************/


	/******************************************************/
#define SERVER_LADDERSEARCHREPLY 0x2fff
	typedef struct /* FIXME: how does client know how many names?
			  do we send separate replies for each name in the request? */
	{
		t_bnet_header h;
		bn_int        rank; /* 0 means 1st, etc */
	} PACKED_ATTR() t_server_laddersearchreply;
#define SERVER_LADDERSEARCHREPLY_RANK_NONE 0xffffffff
	/******************************************************/


	/******************************************************/
	/*
							  FF 30 1C 00 00 00 00 00            .0......
							  32 37 34 34 37 37 32 39   31 34 38 32 38 00 63 6C    2744772914828.cl
							  6F 75 64 00                                          oud.
							  */
#define CLIENT_CDKEY 0x30ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        spawn; /* FIXME: not sure if this is correct, but cdkey2 does it this way */
		/* cd key */
		/* owner name */ /* Was this always here? */
	} PACKED_ATTR() t_client_cdkey;
#define CLIENT_CDKEY_UNKNOWN1 0x00000000
	/******************************************************/


	/******************************************************/
	/*
							  FF 30 0E 00 01 00 00 00            .0......
							  63 6C 6F 75 64 00                                    cloud.
							  */
#define SERVER_CDKEYREPLY 0x30ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        message;
		/* owner name */
	} PACKED_ATTR() t_server_cdkeyreply;
#define SERVER_CDKEYREPLY_MESSAGE_OK       0x00000001
#define SERVER_CDKEYREPLY_MESSAGE_BAD      0x00000002
#define SERVER_CDKEYREPLY_MESSAGE_WRONGAPP 0x00000003
#define SERVER_CDKEYREPLY_MESSAGE_ERROR    0x00000004 /* disabled */
#define SERVER_CDKEYREPLY_MESSAGE_INUSE    0x00000005
	/* (any other value seems to correspond to ok) */
	/******************************************************/


	/******************************************************/
	/*
	FF 36 34 00 00 00 00 00   0D 00 00 00 01 00 00 00    .64.............
	B5 AE 23 00 50 E5 D5 C0   DB 55 1E 38 0A F5 58 B9    ..#.P....U.8..X.
	47 64 C6 C2 9F BB FF B8   81 E7 EB EC 1B 13 C6 38    Gd.............8
	52 6F 62 00                                          Rob.

	FF 36 34 00 00 00 00 00   0D 00 00 00 01 00 00 00    .64.............
	7F D7 00 00 90 64 77 2F   D7 5B 42 38 1F A1 A2 6F    .....dw/.[B8...o
	E8 FA BE F8 B6 0B BA 0F   CA 64 3A 17 14 56 83 AB    .........d:..V..
	42 6F 62 00                                          Bob.

	FF 36 35 00 00 00 00 00   10 00 00 00 04 00 00 00    .65.............
	0D 43 03 00 7A 11 07 ED   7C 9E 1E 38 E5 87 8B 3B    .C..z...|..8...;
	9C 19 91 D9 0D 10 FC C1   C0 86 8C 8D DA A4 45 0B    ..............E.
	XX XX XX XX 00                                       XXXX.

	FF 36 34 00 00 00 00 00   10 00 00 00 04 00 00 00    .64.............
	70 F9 02 00 58 F9 B6 E6   38 49 5C 38 38 9C 31 E4    p...X...8I\88.1.
	1D 3D 40 05 66 AD 4C C8   1D 12 8E 49 9E 60 1A CB    .=@.f.L....I.`..
	42 6F 62 00                                          Bob.
	*/
#define CLIENT_CDKEY2 0x36ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        spawn;
		bn_int        keylen; /* without terminating NUL */
		bn_int        productid;
		bn_int        keyvalue1;
		bn_int        sessionkey;
		bn_int        ticks;
		bn_int        key_hash[5];
		/* owner name */
	} PACKED_ATTR() t_client_cdkey2;
#define CLIENT_CDKEY2_SPAWN_TRUE  0x00000001
#define CLIENT_CDKEY2_SPAWN_FALSE 0x00000000
	/******************************************************/


	/******************************************************/
	/*
	From Diablo II 1.08?
	FF 42 43 00 AB 4C A4 3B            .BC..L.;
	01 00 00 00 00 00 00 00   10 00 00 00 06 00 00 00    ................
	XX 60 12 00 00 00 00 00   5D 82 82 C4 F4 8F D0 91    X`......].......
	E1 5B AB 95 D9 EE EF 18   44 3E F1 C9 XX XX XX XX    .[......D>..XXXX
	XX XX XX XX XX XX XX XX   XX XX 00                   XXXXXXXXXX.

	FF 42 44 00 17 78 42 77   01 00 00 00 00 00 00 00    .BD..xBw........
	10 00 00 00 06 00 00 00   XX F3 10 00 00 00 00 00    ........X.......
	A8 29 8B C4 41 BD 33 AB   74 4C 1F 1E 5C XX CA 83    .)..A.3.tL..\X..
	7F E5 36 14 XX XX XX XX   XX XX XX XX XX XX XX XX    ..6.XXXXXXXXXXXX
	XX XX XX 00                                          XXX.

	FF 42 44 00 C6 25 A1 3B   01 00 00 00 00 00 00 00    .BD..%.;........
	10 00 00 00 06 00 00 00   XX F3 10 00 00 00 00 00    ........X.......
	C4 3F FB 05 94 0C AC D4   3B 63 B1 90 E4 XX 53 B9    .?......;c...XS.
	70 C3 6F 2E XX XX XX XX   XX XX XX XX XX XX XX XX    p.o.XXXXXXXXXXXX
	XX XX XX 00                                          XXX.
	*/
#define CLIENT_CDKEY3 0x42ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        unknown1; /* FIXME: some kind of salt? */
		bn_int        unknown2; /* 01 00 00 00 */
		bn_int        unknown3; /* 00 00 00 00 */
		bn_int        unknown4; /* 10 00 00 00 */
		bn_int        unknown5; /* 06 00 00 00 */
		bn_int        unknown6; /* FIXME: value1? */
		bn_int        unknown7; /* 00 00 00 00 */
		bn_int        key_hash[5];
		/* owner name */
	} PACKED_ATTR() t_client_cdkey3;
#define CLIENT_CDKEY3_UNKNOWN1  0xffffffff
#define CLIENT_CDKEY3_UNKNOWN2  0x00000001
#define CLIENT_CDKEY3_UNKNOWN3  0x00000000
#define CLIENT_CDKEY3_UNKNOWN4  0x00000010
#define CLIENT_CDKEY3_UNKNOWN5  0x00000006
#define CLIENT_CDKEY3_UNKNOWN6  0x00123456
#define CLIENT_CDKEY3_UNKNOWN7  0x00000000
	/******************************************************/


	/******************************************************/
	/*
							  FF 42 09 00 00 00 00 00            .B......
							  00

							  FF 42 09 00 00 00 00 00   00                         .B.......
							  */
#define SERVER_CDKEYREPLY3 0x42ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        message;
		/* owner name */ /* FIXME: or error message, or ... */
	} PACKED_ATTR() t_server_cdkeyreply3;
#define SERVER_CDKEYREPLY3_MESSAGE_OK       0x00000000
	/******************************************************/


	/******************************************************/
	/*
	FF 34 0D 00 00 00 00 00   00 00 00 00 00             .4...........
	*/
#define CLIENT_REALMLISTREQ 0x34ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        unknown1;
		bn_int        unknown2;
	} PACKED_ATTR() t_client_realmlistreq;
	/******************************************************/


	/******************************************************/
	/*
	0000:   FF 40 04 00                                          .@..

	*/
#define CLIENT_REALMLISTREQ_110 0x40ff
	typedef struct
	{
		t_bnet_header h;
	} PACKED_ATTR() t_client_realmlistreq_110;
	/******************************************************/


	/******************************************************/
	/*
	FF 34 5E 00 00 00 00 00   01 00 00 00 00 00 00 C0    .4^.............
	00 00 00 00 00 00 00 00   00 00 00 00 10 82 01 00    ................
	FF FF FF FF 00 00 00 00   42 65 74 61 57 65 73 74    ........BetaWest
	00 50 6C 65 61 73 65 20   73 65 6C 65 63 74 20 74    .Please select t
	68 69 73 20 61 73 20 79   6F 75 72 20 72 65 61 6C    his as your real
	6D 20 64 75 72 69 6E 67   20 62 65 74 61 00          m during beta.

	ff 34 5e 00 00 00 00 00   01 00 00 00 00 00 00 c0    .4^.............
	00 00 00 00 00 00 00 00   00 00 00 00 bc 95 01 00    ................
	ff ff ff ff 00 00 00 00   42 65 74 61 57 65 73 74    ........BetaWest
	00 50 6c 65 61 73 65 20   73 65 6c 65 63 74 20 74    .Please select t
	68 69 73 20 61 73 20 79   6f 75 72 20 72 65 61 6c    his as your real
	6d 20 64 75 72 69 6e 67   20 62 65 74 61 00          m during beta.

	ff 34 5e 00 00 00 00 00   01 00 00 00 00 00 00 c0    .4^.............
	00 00 00 00 00 00 00 00   00 00 00 00 c8 99 01 00    ................
	ff ff ff ff 00 00 00 00   42 65 74 61 57 65 73 74    ........BetaWest
	00 50 6c 65 61 73 65 20   73 65 6c 65 63 74 20 74    .Please select t
	68 69 73 20 61 73 20 79   6f 75 72 20 72 65 61 6c    his as your real
	6d 20 64 75 72 69 6e 67   20 62 65 74 61 00          m during beta.

	from bnetd-0.3.23pre18 to Diablo II 1.03
	FF 34 4B 00 00 00   00 00 01 00 00 00 00 00      .4K...........
	00 C0 00 00 00 00 00 00   00 00 00 00 00 00 10 82    ................
	01 00 FF FF FF FF 00 00   00 00 51 61 72 61 74 68    ..........Qarath
	52 65 61 6C 6D 00 54 48   45 20 43 68 6F 69 63 65    Realm.THE Choice
	20 46 6F 72 20 4E 6F 77   28 74 6D 29 00              For Now(tm).
	*/
#define SERVER_REALMLISTREPLY 0x34ff /* realm list reply? */
	typedef struct
	{
		t_bnet_header h;
		bn_int        unknown1;
		bn_int        count;
		/* realm entries */
	} PACKED_ATTR() t_server_realmlistreply;
#define SERVER_REALMLISTREPLY_UNKNOWN1 0x00000000

	typedef struct
	{
		bn_int unknown3;
		bn_int unknown4;
		bn_int unknown5;
		bn_int unknown6;
		bn_int unknown7; /* this one is always different... 00 01 XX XX.. what is it? */
		bn_int unknown8;
		bn_int unknown9;
		/* realm name */
		/* realm description */
	} PACKED_ATTR() t_server_realmlistreply_data;
#define SERVER_REALMLISTREPLY_DATA_UNKNOWN3 0xc0000000
#define SERVER_REALMLISTREPLY_DATA_UNKNOWN4 0x00000000
#define SERVER_REALMLISTREPLY_DATA_UNKNOWN5 0x00000000
#define SERVER_REALMLISTREPLY_DATA_UNKNOWN6 0x00000000
#define SERVER_REALMLISTREPLY_DATA_UNKNOWN7 0x00018210 /* 98832 or 1;33296 */
#define SERVER_REALMLISTREPLY_DATA_UNKNOWN8 0xffffffff
#define SERVER_REALMLISTREPLY_DATA_UNKNOWN9 0x00000000
	/******************************************************/



	/******************************************************/
	/*
	# 44 packet from server: type=0x40ff(unknown) length=40 class=bnet
	0000:   FF 40 28 00 00 00 00 00   01 00 00 00 01 00 00 00    .@(.............
	0010:   45 75 72 6F 70 65 00 52   65 61 6C 6D 20 66 6F 72    Europe.Realm for
	0020:   20 45 75 72 6F 70 65 00                               Europe.
	*/
#define SERVER_REALMLISTREPLY_110 0x40ff

	typedef struct
	{
		t_bnet_header h;
		bn_int        unknown1;
		bn_int        count;
		/* realm entries */
	} PACKED_ATTR() t_server_realmlistreply_110;
#define SERVER_REALMLISTREPLY_110_UNKNOWN1 0x00000000

	typedef struct
	{
		bn_int unknown1;
		/* realm name */
		/* realm description */
	} PACKED_ATTR() t_server_realmlistreply_110_data;
#define SERVER_REALMLISTREPLY_110_DATA_UNKNOWN1 0x00000001


#define CLIENT_PROFILEREQ 0x35ff
	typedef struct /* join realm request */
	{
		t_bnet_header h;
		bn_int	  count;
		/* player name */
	} PACKED_ATTR() t_client_profilereq;


#define SERVER_PROFILEREPLY 0x35ff
	typedef struct /* realm join reply? */
	{
		t_bnet_header h;
		bn_int	  count;	/* same as in req */
		bn_byte	  fail;		/* != 0 if a problem occured */
		/* profile-description */
		/* profile-location */
		/* bn_int clanTAG */
	} PACKED_ATTR() t_server_profilereply;

#define CLIENT_UNKNOWN_37 0x37ff
	typedef struct /* character list request, character list upload? */
	{
		t_bnet_header h;
		bn_int        opencount;  /* Number of OPEN characters on user's machine!    */
		/* Always zero for "closed" connections.           */
		/* unknown2 */            /* subsequent blocks of t_d2char_info or something */
		/* similar, so server could read this list and     */
		/* include in the 0x37ff reply as a choice (this   */
		/* makes sense cuz the server does NOT store open  */
		/* character details - this also explains why      */
		/* unknown1 is always 0 in the beta, and the 0x00  */
		/* of unknown2 acts as a EOF when client read the  */
		/* t_d2char_info structures                        */
	} PACKED_ATTR() t_client_unknown_37;
	/******************************************************/


	/******************************************************/
	/*
	FF 37 01 01 00 00 00 00   08 00 00 00 04 00 00 00    .7..............
	42 65 74 61 57 65 73 74   2C 4D 6F 4E 6B 00 87 80    BetaWest,MoNk...
	06 01 01 01 01 50 FF FF   02 02 FF FF FF FF FF FF    .....P..........
	02 49 50 50 50 50 FF FF   FF 50 50 FF FF FF FF FF    .IPPPP...PP.....
	FF 14 88 82 80 80 FF FF   FF 00 42 65 74 61 57 65    ..........BetaWe
	73 74 2C 4D 6F 4E 6B 2D   65 00 83 80 05 02 02 01    st,MoNk-e.......
	01 2B FF 1B 02 02 FF FF   FF FF FF FF 03 FF FF FF    .+..............
	FF FF FF FF A8 FF FF FF   FF FF FF FF FF 10 80 82    ................
	80 80 FF FF FF 00 42 65   74 61 57 65 73 74 2C 4D    ......BetaWest,M
	6F 4E 6B 2D 65 65 00 83   80 06 01 01 01 01 FF 4C    oNk-ee.........L
	FF 02 02 FF FF FF FF FF   FF 01 FF 48 48 48 48 FF    ...........HHHH.
	A6 FF 48 48 FF FF FF FF   FF FF 0F 80 80 80 80 FF    ..HH............
	FF FF 00 42 65 74 61 57   65 73 74 2C 4D 6F 4E 6B    ...BetaWest,MoNk
	2D 74 77 6F 00 87 80 01   01 01 01 01 FF FF FF 01    -two............
	01 FF FF FF FF FF FF 02   FF FF FF FF FF FF FF FF    ................
	FF FF FF FF FF FF FF FF   01 84 80 FF FF FF 80 80    ................
	00                                                   .

	^-- 1: (BetaWest) MoNk
	2: (BetaWest) MoNk-e
	3: (BetaWest) MoNk-ee
	4: (BetaWest) MoNk-two

	ff 37 4e 00 00 00 00 00   08 00 00 00 01 00 00 00    .7N.............
	42 65 74 61 57 65 73 74   2c 4c 69 66 65 6c 69 6b    BetaWest,Lifelik
	65 00 87 80 01 01 01 01   01 ff ff ff 01 01 ff ff    e...............
	ff ff ff ff 03 ff ff ff   ff ff ff ff ff ff ff ff    ................
	ff ff ff ff ff 01 80 80   ff ff ff 80 80 00          ..............

	ff 37 4e 00 00 00 00 00   08 00 00 00 01 00 00 00    .7N.............
	42 65 74 61 57 65 73 74   2c 51 6c 65 78 54 45 53    BetaWest,QlexTES
	54 00 83 80 ff ff ff ff   ff 30 ff 1b ff ff ff ff    T........0......
	ff ff ff ff 04 ff ff ff   ff ff ff ff ff ff ff ff    ................
	ff ff ff ff ff 01 80 80   80 80 ff ff ff 00          ..............

	from bnetd-0.3.23pre18 to Diablo II 1.03
	"Char1 {BNE}" [lvl 20, amaz]
	"Char2 {BNE}" [lvl 21, sorc]
	"Char3 {BNE}" [lvl 22, necro]
	FF 37 D9 00 00 00   00 00 08 00 00 00 03 00    Gv.7............
	00 00 51 61 72 61 74 68   52 65 61 6C 6D 2C 43 68    ..QarathRealm,Ch
	61 72 31 00 87 80 01 01   01 01 01 01 01 01 01 01    ar1.............
	01 01 01 01 01 01 01 01   01 01 01 01 01 01 01 01    ................
	01 01 01 01 01 01 01 14   85 86 01 FF FF FF FF 42    ...............B
	4E 45 54 44 00 51 61 72   61 74 68 52 65 61 6C 6d    NETD.QarathRealm
	2C 43 68 61 72 32 00 87   80 01 01 01 01 01 01 01    ,Char2..........
	01 01 01 01 01 01 01 01   01 02 01 01 01 01 01 01    ................
	01 01 01 01 01 01 01 01   01 01 15 85 86 01 FF FF    ................
	FF FF 42 4E 45 54 44 00   51 61 72 61 74 68 52 65    ..BNETD.QarathRe
	61 6C 6D 2C 43 68 61 72   33 00 87 80 01 01 01 01    alm,Char3.......
	01 01 01 01 01 01 01 01   01 01 01 01 03 01 01 01    ................
	01 01 01 01 01 01 01 01   01 01 01 01 01 16 85 86    ................
	01 FF FF FF FF 42 4E 45   54 44 00                   .....BNETD.
	*/
#define SERVER_UNKNOWN_37 0x37ff
	typedef struct /* character list reply? */
	{
		t_bnet_header h;
		bn_int        unknown1;
		bn_int        unknown2; /* _bucky_: max chars allowed? */
		bn_int        count;    /* # of chars, same number of  */
		/* t_char_info to follow in    */
		/* packet                      */
		/* d2char_info blocks */
	} PACKED_ATTR() t_server_unknown_37;
#define SERVER_UNKNOWN_37_UNKNOWN1 0x00000000
#define SERVER_UNKNOWN_37_UNKNOWN2 0x00000008

	/* The ONLY 0x00 that should appear should be the terminating NUL for  */
	/* the character name string and the guild tag string, they're used as */
	/* delimiters to separate character name and the character structure   */
	/* If you got any other NUL's in here the next character's info will   */
	/* be royally fucked up - using 0x01 or 0xff for unknowns seem to work */
	/* well                                                                */
	typedef struct
	{
		/* "RealmName,CharacterName" - for closed characters */
		/* - OR -                                            */
		/* "CharacterName" - for open characters             */
		/* - strlen(CharacterName) must be <= 15 -           */
		bn_byte unknownb1;     /* 0x83, 0x87? */
		bn_byte unknownb2;     /* 0x80...? */
		bn_byte helmgfx;
		bn_byte bodygfx;
		bn_byte leggfx;
		bn_byte lhandweapon;
		bn_byte lhandgfx;
		bn_byte rhandweapon;

		/* Partial weapon code list:
				  0x2f: 1H Axe
				  0x30: 1H Sword
				  0x50: 2H Staff
				  0x51: Another 2H Staff
				  0x52: Another 2H Staff
				  0x53: Another 2H Staff
				  0x54: 2H Axe
				  0x55: Scythe
				  0x56: empty?
				  0x57: Another 2H Axe
				  0x58: Halberd?
				  0x59: empty?
				  0x5a: Another 2H Axe
				  0x5b: Another Halberd
				  0x5c: empty?
				  0x5d: 1H club?
				  0x5e: empty?
				  0x5f: empty?
				  */

		bn_byte rhandgfx;
		bn_byte unknownb3;
		bn_byte unknownb4;
		bn_byte unknownb5;
		bn_byte unknownb6;
		bn_byte unknownb7;
		bn_byte unknownb8;
		bn_byte unknownb9;
		bn_byte unknownb10;
		bn_byte unknownb11;
		bn_byte chclass;     /* 0x01=Amazon, 0x02=Sor, 0x03=Nec, 0x04=Pal, 0x05=Bar */

		bn_int  unknown1;
		bn_int  unknown2;
		bn_int  unknown3;
		bn_int  unknown4;

		bn_byte level;     /* yes, byte, not short/int/long  */
		bn_byte status;    /* 0x01-03 = Norm & alive         */
		/* 0x04-07 = HC & alive           */
		/* 0x08-0b = Norm & "dead"?       */
		/* 0x0c+   = HC & dead, chat only */
		/* Add 0x80 to get same effect    */
		bn_byte title;     /* 0x01=none
							  0x02=Sir/Dame?
							  0x03=Sir/Dame?
							  0x04=Lord?
							  0x05=Lord?
							  0x06=Baron?
							  0x07=Baron? */
		/* Same codes for HC chars                            */
		/* Add 0x80 to get same effect    */
		bn_byte unknownb13;
		bn_byte emblembgc; /* Guild emblem background colour */
		bn_byte emblemfgc; /* Guild emblem foreground colour */
		bn_byte emblemnum; /* Guild emblem type number       */

		/* emblem number corresponds to D2DATA.MPQ/data/global/ui/Emblems/iconXXa.dc6 */
		/* where XX = emblem number - 1 (ie, 0x0A corresponds to icon09a.dc6) use     */
		/* for dummy values seem safe... 0x01 won't work, you'll get an emblem...     */

		bn_byte unknownb14;
		/* Guild Tag */ /* must not be longer than 3 chars */
	} PACKED_ATTR() t_d2char_info;
#define D2CHAR_INFO_UNKNOWNB1 0x83
#define D2CHAR_INFO_UNKNOWNB2 0x80
#define D2CHAR_INFO_FILLER 0xff /* non-zero padding */
#define D2CHAR_INFO_CLASS_AMAZON      0x01
#define D2CHAR_INFO_CLASS_SORCERESS   0x02
#define D2CHAR_INFO_CLASS_NECROMANCER 0x03
#define D2CHAR_INFO_CLASS_PALADIN     0x04
#define D2CHAR_INFO_CLASS_BARBARIAN   0x05
#define D2CHAR_INFO_CLASS_DRUID       0x06
#define D2CHAR_INFO_CLASS_ASSASSIN    0x07
	/******************************************************/


	/******************************************************/
	/* D2 packet... not sent very often and the client doesn't
	 * seem to expect an answer */
	/* FIXME: what the hell does this one do? */
	/*
	FF 39 13 00 42 65 74 61   57 65 73 74 2C 62 75 73    .9..BetaWest,bus
	74 61 00                                             ta.

	this one was sent after a closed character was deleted on the auth
	server... maybe a notifier for the gateway server?
	FF 39 17 00 42 6F 62 73   57 6F 72 6C 64 2C 63 68    .9..BobsWorld,ch
	61 72 6E 61 6D 65 00                                 arname.
	*/
#define CLIENT_UNKNOWN_39 0x39ff
	typedef struct
	{
		t_bnet_header h;
		/* character name */ /* what about open chars? */
	} PACKED_ATTR() t_client_unknown_39;
	/******************************************************/


	/******************************************************/
	/*
	FF 3A 2E 00 58 4C F2 00   19 C2 08 00 D7 33 37 D3    .:..XL.......37.
	42 8C 92 37 C2 26 08 A9   3E 92 05 28 A1 5A 18 B9    B..7.&..>..(.Z..
	6D 61 73 74 6F 64 6F 6E   74 66 69 6C 6D 00          mastodontfilm.

	FF 3A 28 00 2B 73 1C 01   88 91 F2 0D AF 22 43 25    .:(.+s......."C%
	BF E4 2D 45 42 37 04 DB   AF 95 66 71 16 85 67 60    ..-EB7....fq..g`
	51 6C 65 78 53 5A 47 00                              QlexSZG.
	*/
#define CLIENT_LOGINREQ2 0x3aff
	typedef struct
	{
		t_bnet_header h;
		bn_int        ticks; /* is it really? */
		bn_int        sessionkey;
		bn_int        password_hash2[5];
		/* player name */
	} PACKED_ATTR() t_client_loginreq2;
	/******************************************************/


	/******************************************************/
	/*
	# 21 packet from client: type=0x46ff(unknown) length=8 class=bnet
	0000:   FF 46 08 00 00 00 00 00                              .F......
	*/
#define CLIENT_MOTD_W3 0x46ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        last_news_time; /* date of the last news item the client has */
	} PACKED_ATTR() t_client_motd_w3;
	/******************************************************/


	/******************************************************/
	/*
	# 22 packet from server: type=0x46ff(unknown) length=225 class=bnet
	0000:   FF 46 E1 00 01 16 3A 6C   3C FF FF FF FF 00 00 00    .F....:l<.......
	0010:   00 00 00 00 00 57 65 6C   63 6F 6D 65 20 74 6F 20    .....Welcome to
	0020:   42 61 74 74 6C 65 2E 6E   65 74 21 0A 54 68 69 73    Battle.net!.This
	0030:   20 73 65 72 76 65 72 20   69 73 20 68 6F 73 74 65     server is hoste
	0040:   64 20 62 79 20 41 54 26   54 2E 0A 54 68 65 72 65    d by AT&T..There
	0050:   20 61 72 65 20 63 75 72   72 65 6E 74 6C 79 20 36     are currently 6
	0060:   32 38 20 75 73 65 72 73   20 70 6C 61 79 69 6E 67    28 users playing
	0070:   20 31 35 39 20 67 61 6D   65 73 20 6F 66 20 57 61     159 games of Wa
	0080:   72 63 72 61 66 74 20 49   49 49 2C 20 61 6E 64 20    rcraft III, and
	0090:   31 37 37 33 34 36 20 75   73 65 72 73 20 70 6C 61    177346 users pla
	00A0:   79 69 6E 67 20 37 37 38   33 37 20 67 61 6D 65 73    ying 77837 games
	00B0:   20 6F 6E 20 42 61 74 74   6C 65 2E 6E 65 74 2E 0A     on Battle.net..
	00C0:   4C 61 73 74 20 6C 6F 67   6F 6E 3A 20 54 68 75 20    Last logon: Thu
	00D0:   46 65 62 20 31 34 20 20   35 3A 32 38 20 50 4D 0A    Feb 14  5:28 PM.
	00E0:   00                                                   .

	# Match 4, 2002
	# 92 packet from server: type=0x46ff(unknown) length=859 class=bnet
	0000:   FF 46 5B 03 01 B4 B2 82   3C 20 B6 83 3C 20 B6 83    .F[.....< ..< ..
	0010:   3C 20 B6 83 3C 57 65 20   68 61 76 65 20 62 65 65    < ..<We have bee
	# 93 packet from server: type=0x46ff(unknown) length=223 class=bnet
	0000:   FF 46 DF 00 01 B4 B2 82   3C 20 B6 83 3C 20 B6 83    .F......< ..< ..
	0010:   3C 00 00 00 00 57 65 6C   63 6F 6D 65 20 74 6F 20    <....Welcome to

	*/
#define SERVER_MOTD_W3 0x46ff
	typedef struct
	{
		t_bnet_header h;
		bn_byte        msgtype; /* we only saw "1" type so far */
		bn_int         curr_time; /* as seen by the server */
		bn_int         first_news_time; /* the oldest news item's timestamp */
		bn_int         timestamp; /* the timestamp of this news item */
		/* it is equal with the latest news item timestamp for
		the welcome message */
		bn_int         timestamp2; /* always equal with the timestamp except the
						last packet which shows in the right panel */
		/* text */
	} PACKED_ATTR() t_server_motd_w3;
#define SERVER_MOTD_W3_MSGTYPE  0x01
#define SERVER_MOTD_W3_WELCOME  0x00000000
	/******************************************************/

	/******************************************************/
	/*
	# Jon/bbbbb
	# 28 packet from client: type=0x53ff(unknown) length=40 class=bnet
	0000:   FF 53 28 00 6F FD 5F 61   C3 D1 C4 78 E6 2E 24 8B    .S(.o._a...x..$.
	0010:   32 EB 36 9C 39 57 D8 BA   57 84 67 5E E7 78 5B 01    2.6.9W..W.g^.x[.
	0020:   6D 99 87 15 4A 6F 6E 00                              m...Jon.
	*/
#define CLIENT_LOGINREQ_W3 0x53ff
	typedef struct
	{
		t_bnet_header h;
		bn_byte        client_public_key[32];
		/* player name */
	} PACKED_ATTR() t_client_loginreq_w3;
	/******************************************************/

	/******************************************************/
	/*
	12:33:56.255569 63.241.83.11.6112 > ws-2-11.1038: P 190:262(72) ack 272 win 65264
	** ** ** ** ** ** ** **   ** ** ** ** ** ** ** **
	** ** ** ** ** ** ** **   ** ** ** ** ** ** ** **
	** ** ** ** ** ** ** **   FF 53 48 00 00 00 00 00            .SH.....
	4B A8 FF 5D 1E 5D 2D 50   D1 2B B2 95 74 AD 5F 4E    K..].]-P.+..t._N
	88 A4 88 48 18 27 89 50   F1 AA 1B D5 D7 B6 47 BC    ...H.'.P......G.
	30 8B 2A 54 AA 99 23 96   75 8A 5E 67 35 8E 5B 22    0.*T..#.u.^g5.["
	2C 0E 68 2E C2 95 E9 D7   A1 82 F1 2C 1E 2B 28 36    ,.h........,.+(6
	*/
#define SERVER_LOGINREPLY_W3 0x53ff
	typedef struct
	{
		t_bnet_header h;
		bn_int       message;
		/* seems to be response to client-challenge */
		bn_byte       salt[32];
		bn_byte       server_public_key[32];
	} PACKED_ATTR() t_server_loginreply_w3;
#define SERVER_LOGINREPLY_W3_MESSAGE_SUCCESS 0x00000000
#define SERVER_LOGINREPLY_W3_MESSAGE_ALREADY 0x00000001 /* Account already logged on */
#define SERVER_LOGINREPLY_W3_MESSAGE_BADACCT 0x00000001 /* Accoutn does not exist */
	/******************************************************/

	/******************************************************/
	/* single player crack based:
	# 34 packet from server: type=0x54ff(unknown) length=40 class=bnet
	0000:   FF 54 28 00 00 00 00 00   00 00 00 00 00 00 00 00    .T(.............
	0010:   00 00 00 00 00 00 00 00   00 00 00 00 00 00 00 00    ................
	0020:   00 00 00 00 00 00 00 00                              ........

	* Password Checksum ? *
	-- client --
	0x54ff - 2 bytes
	size - 2 bytes (0x0018)
	unknown1 - 20 bytes
	-- server --
	0x54ff - 2 bytes
	size - 2 bytes
	msgid - 4 bytes
	{
	0x00000000	accept
	0x00000002	password incorrect
	}
	unknown1 - 20 bytes

	Packet #13
	0x0000   FF 54 1C 00 00 00 00 00-3A D5 B9 B1 2B D9 B5 D9   T......:չ?ٵ?
	0x0010   87 3B 2B 3D 28 57 C0 2E-02 93 5F 8B               ?+=(W?._?
	*/
#define CLIENT_LOGONPROOFREQ 0x54ff
	typedef struct
	{
		t_bnet_header h;
		bn_byte	client_password_proof[20];
	} PACKED_ATTR() t_client_logonproofreq;

#define SERVER_LOGONPROOFREPLY 0x54ff
	typedef struct
	{
		t_bnet_header h;
		bn_int	response;
		bn_byte	server_password_proof[20];
	} PACKED_ATTR() t_server_logonproofreply;
#define SERVER_LOGONPROOFREPLY_RESPONSE_OK      0x00000000
#define SERVER_LOGONPROOFREPLY_RESPONSE_BADPASS 0x00000002
#define SERVER_LOGONPROOFREPLY_RESPONSE_EMAIL   0x0000000E
#define SERVER_LOGONPROOFREPLY_RESPONSE_CUSTOM  0x0000000F
	/******************************************************/

	/******************************************************/
#define CLIENT_PASSCHANGEREQ 0x55ff
	typedef struct
	{
		t_bnet_header h;
		bn_byte client_public_key[32];
		/* username */
	} PACKED_ATTR() t_client_passchangereq;

#define SERVER_PASSCHANGEREPLY 0x55ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        message;
		bn_byte       salt[32];
		bn_byte       server_public_key[32];
	} PACKED_ATTR() t_server_passchangereply;
#define SERVER_PASSCHANGEREPLY_MESSAGE_ACCEPT 0x00000000
#define SERVER_PASSCHANGEREPLY_MESSAGE_REJECT 0x00000001 /* No such account */
	/******************************************************/

	/******************************************************/
#define CLIENT_PASSCHANGEPROOFREQ 0x56ff
	typedef struct
	{
		t_bnet_header h;
		bn_byte       client_password_proof[20];
		bn_byte       salt[32];
		bn_byte       password_verifier[32];
	} PACKED_ATTR() t_client_passchangeproofreq;

#define SERVER_PASSCHANGEPROOFREPLY 0x56ff
	typedef struct
	{
		t_bnet_header h;
		bn_int	response;
		bn_byte       server_password_proof[20];
	} PACKED_ATTR() t_server_passchangeproofreply;
#define SERVER_PASSCHANGEPROOFREPLY_RESPONSE_OK      0x00000000
#define SERVER_PASSCHANGEPROOFREPLY_RESPONSE_BADPASS 0x00000002
	/******************************************************/

	/******************************************************/
	/*
	# 13 packet from client: type=0x52ff(unknown) length=83 class=bnet
	0000:   FF 52 53 00 2B 63 B9 05   CA F3 E1 BA 58 5C ED 65    .RS.+c......X\.e
	0010:   BE 8F 0E 89 A9 B8 C7 FE   75 2C 44 10 AE 19 B5 14    ........u,D.....
	0020:   E8 CA E9 C7 37 50 7D 0F   9A 89 00 FF 2F 10 BB EE    ....7P}...../...
	0030:   A8 0C 81 64 AD AF DC C7   3F 58 F1 20 A1 05 E2 38    ...d....?X. ...8
	0040:   18 87 85 5B 74 68 65 61   63 63 6F 75 6E 74 6E 61    ...[theaccountna
	0050:   6D 65 00                                             me.
	*/
#define CLIENT_CREATEACCOUNT_W3 0x52ff
	typedef struct
	{
		t_bnet_header h;
		bn_byte       salt[32];
		bn_byte       password_verifier[32];
		/* player name */
	} PACKED_ATTR() t_client_createaccount_w3;
	/******************************************************/


	/******************************************************/

	/******************************************************/

	/*
	# 20 packet from client: type=0x45ff(unknown) length=6 class=bnet
	0000:   FF 45 06 00 E0 17                                    .E....
	*/

} /* namespace pvpgn */

#endif /* INCLUDED_BNET_PROTOCOL_AUTH_H */
