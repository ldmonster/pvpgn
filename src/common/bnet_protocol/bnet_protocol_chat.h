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

/* Chat packets: CHANGEGAMEPORT, JOINCHANNEL, CHANNELLIST, ENTERCHAT, etc. */
#ifndef INCLUDED_BNET_PROTOCOL_CHAT_H
#define INCLUDED_BNET_PROTOCOL_CHAT_H

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

#define CLIENT_CHANGEGAMEPORT 0x45ff
	typedef struct
	{
		t_bnet_header	h;
		bn_short	port;
	} PACKED_ATTR() t_client_changegameport;




	/******************************************************/
	/*
	RECV-> 0000   FF 52 08 00 00 00 00 00                            .R......
	*/
#define SERVER_CREATEACCOUNT_W3 0x52ff
	typedef struct
	{
		t_bnet_header h;
		bn_int       result;
	} PACKED_ATTR() t_server_createaccount_w3;
#define SERVER_CREATEACCOUNT_W3_RESULT_OK    0x00000000
#define SERVER_CREATEACCOUNT_W3_RESULT_EXIST 0x00000004
#define SERVER_CREATEACCOUNT_W3_RESULT_EMPTY 0x00000007
#define SERVER_CREATEACCOUNT_W3_RESULT_INVALID 0x00000008
#define SERVER_CREATEACCOUNT_W3_RESULT_BANNED 0x00000009
#define SERVER_CREATEACCOUNT_W3_RESULT_SHORT 0x0000000A
#define SERVER_CREATEACCOUNT_W3_RESULT_PUNCTUATION 0x0000000B
#define SERVER_CREATEACCOUNT_W3_RESULT_PUNCTUATION2 0x0000000C
	/******************************************************/

	/******************************************************/
	/*
	FF 3A 08 00 00 00 00 00                              .:......
	*/
#define SERVER_LOGINREPLY2 0x3aff
	typedef struct
	{
		t_bnet_header h;
		bn_int        message;
		/* reason for account lock */
	} PACKED_ATTR() t_server_loginreply2;
#define SERVER_LOGINREPLY2_MESSAGE_SUCCESS  0x00000000
#define SERVER_LOGINREPLY2_MESSAGE_NONEXIST 0x00000001 /* Account does not exist */
#define SERVER_LOGINREPLY2_MESSAGE_BADPASS  0x00000002 /* Bad password */
#define SERVER_LOGINREPLY2_MESSAGE_LOCKED   0x00000006 /* Account is locked */
	/******************************************************/


	/******************************************************/
	/* Diablo II 1.03 */
	/* sent when registering new player with open Battle.net */
	/* and when logging in with closed Battle.net */
	/*
	(closed) b.net login:
	enter name/password -> 3dff packet
	(open) b.net login:
	enter name/password -> "login" packet (bad account) <-- CORRECT
	"Create new account" -> TOS grab -> Enter password -> 3dff packet
	*/
	/*
	FF 3D 20 00 B8 C0 A1 2F   56 B1 47 65 CF 55 09 62    .= ..../V.Ge.U.b
	8E 21 3C 59 57 BC E8 EA   45 6C 66 6C 6F 72 64 00    .!<YW...Elflord.
	*/
#define CLIENT_CREATEACCTREQ2 0x3dff
	typedef struct
	{
		t_bnet_header h;
		bn_int        password_hash1[5];
		/* username (charactername?) */
	} PACKED_ATTR() t_client_createacctreq2;
	/******************************************************/


	/******************************************************/
	/* Diablo II 1.03 */
#define SERVER_CREATEACCTREPLY2 0x3dff
	typedef struct
	{
		t_bnet_header h;
		bn_int        result;
	} PACKED_ATTR() t_server_createacctreply2;
#define SERVER_CREATEACCTREPLY2_RESULT_OK    0x00000000
#define SERVER_CREATEACCTREPLY2_RESULT_SHORT 0x00000001 /* Username must be a minimum of 2 characters */
#define SERVER_CREATEACCTREPLY2_RESULT_INVALID 0x00000002
#define SERVER_CREATEACCTREPLY2_RESULT_BANNED 0x00000003
#define SERVER_CREATEACCTREPLY2_RESULT_EXIST 0x00000004
#define SERVER_CREATEACCTREPLY2_RESULT_LAST_CREATE_IN_PROGRESS 0x00000005
#define SERVER_CREATEACCTREPLY2_RESULT_ALPHANUM 0x00000006
#define SERVER_CREATEACCTREPLY2_RESULT_PUNCTUATION 0x00000007
#define SERVER_CREATEACCTREPLY2_RESULT_PUNCTUATION2 0x00000008
	/******************************************************/


	/******************************************************/
	/*
	FF 36 0C 00 01 00 00 00   52 6F 62 00                .6......Rob.

	FF 36 0C 00 01 00 00 00   42 6F 62 00                .6......Bob.
	*/
#define SERVER_CDKEYREPLY2 0x36ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        message;
		/* owner name */
	} PACKED_ATTR() t_server_cdkeyreply2;
#define SERVER_CDKEYREPLY2_MESSAGE_OK       0x00000001
#define SERVER_CDKEYREPLY2_MESSAGE_BAD      0x00000002
#define SERVER_CDKEYREPLY2_MESSAGE_WRONGAPP 0x00000003
#define SERVER_CDKEYREPLY2_MESSAGE_ERROR    0x00000004
#define SERVER_CDKEYREPLY2_MESSAGE_INUSE    0x00000005
	/* (any other value seems to correspond to ok) */
	/******************************************************/


	/******************************************************/
	/*
	FF 14 08 00 74 65 6E 62                              ....tenb
	*/
#define CLIENT_UDPOK 0x14ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        echo; /* echo of what the server sent, normally "tenb" */
	} PACKED_ATTR() t_client_udpok;
	/******************************************************/


	/******************************************************/
	/*
	FF 33 18 00 1A 00 00 00   00 00 00 00 74 6F 73 5F    .3..........tos_
	55 53 41 2E 74 78 74 00                              USA.txt.

	Diablo II 1.03
	FF 33 19 00 04 00   00 80 00 00 00 00 62 6E      .3..........bn
	73 65 72 76 65 72 2E 69   6E 69 00                   server.ini.
	*/
#define CLIENT_FILEINFOREQ 0x33ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        type;     /* type of file (TOS,icons,etc.) */
		bn_int        unknown2; /* 00 00 00 00 */ /* always zero? */
		/* filename */          /* default/suggested filename? */
	} PACKED_ATTR() t_client_fileinforeq;
#define CLIENT_FILEINFOREQ_TYPE_TOS          0x0000001a /* tos_USA.txt */
#define CLIENT_FILEINFOREQ_TYPE_GATEWAYS     0x0000001b /* STAR bnserver.ini */
#define CLIENT_FILEINFOREQ_TYPE_GATEWAYS_D2  0x80000004 /* D2XP bnserver-D2DV.ini */
	/* FIXME: what about icons.bni? */
#define CLIENT_FILEINFOREQ_TYPE_ICONS        0x0000001d /* STAR icons_STAR.bni */
#define CLIENT_FILEINFOREQ_UNKNOWN2          0x00000000
#define CLIENT_FILEINFOREQ_FILE_TOSUSA        "tos_USA.txt"
#define CLIENT_FILEINFOREQ_FILE_TOSUNICODEUSA "tos-unicode_USA.txt"
#define CLIENT_FILEINFOREQ_FILE_BNSERVER      "bnserver.ini"
	/******************************************************/


	/******************************************************/
	/*
	FF 33 1C 00 1A 00 00 00   00 00 00 00 30 C3 89 86    .3..........0...
	09 4F BD 01 74 6F 73 2E   74 78 74 00                .O..tos.txt.

	FF 33 20 00 1A 00 00 00   00 00 00 00 00 38 51 E2    .3 ..........8Q.
	30 A1 BD 01 74 6F 73 5F   55 53 41 2E 74 78 74 00    0...tos_USA.txt.
	*/
#define SERVER_FILEINFOREPLY 0x33ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        type;      /* type of file (TOS,icons,etc.) */
		bn_int        unknown2;  /* 00 00 00 00 */ /* same as in TOSREQ */
		bn_long       timestamp; /* file modification time */
		/* filename */
	} PACKED_ATTR() t_server_fileinforeply;
#define SERVER_FILEINFOREPLY_TYPE_TOSFILE      0x0000001a /* tos_USA.txt */
#define SERVER_FILEINFOREPLY_TYPE_GATEWAYS     0x0000001b /* STAR bnserver.ini */
#define SERVER_FILEINFOREPLY_TYPE_GATEWAYS_D2  0x80000004 /* D2XP bnserver-D2DV.ini */
#define SERVER_FILEINFOREPLY_TYPE_EXTRAWORK    0x80000005 /* IX86ExtraWork.mpq */
#define SERVER_FILEINFOREPLY_TYPE_ICONS        0x0000001d /* STAR icons_STAR.bni */
#define SERVER_FILEINFOREPLY_UNKNOWN2          0x00000000 /* always zero */
	/******************************************************/


	/******************************************************/
	/*
				FF 26 9E 01   01 00 00 00 13 00 00 00        .&..........
				78 52 82 02 42 6F 62 00   70 72 6F 66 69 6C 65 5C    xR..Bob.profile\
				73 65 78 00 70 72 6F 66   69 6C 65 5C 61 67 65 00    sex.profile\age.
				70 72 6F 66 69 6C 65 5C   6C 6F 63 61 74 69 6F 6E    profile\location
				00 70 72 6F 66 69 6C 65   5C 64 65 73 63 72 69 70    .profile\descrip
				74 69 6F 6E 00 52 65 63   6F 72 64 5C 53 45 58 50    tion.Record\SEXP
				5C 30 5C 77 69 6E 73 00   52 65 63 6F 72 64 5C 53    \0\wins.Record\S
				45 58 50 5C 30 5C 6C 6F   73 73 65 73 00 52 65 63    EXP\0\losses.Rec
				6F 72 64 5C 53 45 58 50   5C 30 5C 64 69 73 63 6F    ord\SEXP\0\disco
				6E 6E 65 63 74 73 00 52   65 63 6F 72 64 5C 53 45    nnects.Record\SE
				58 50 5C 30 5C 6C 61 73   74 20 67 61 6D 65 00 52    XP\0\last game.R
				65 63 6F 72 64 5C 53 45   58 50 5C 30 5C 6C 61 73    ecord\SEXP\0\las
				74 20 67 61 6D 65 20 72   65 73 75 6C 74 00 52 65    t game result.Re
				63 6F 72 64 5C 53 45 58   50 5C 31 5C 77 69 6E 73    cord\SEXP\1\wins
				00 52 65 63 6F 72 64 5C   53 45 58 50 5C 31 5C 6C    .Record\SEXP\1\l
				6F 73 73 65 73 00 52 65   63 6F 72 64 5C 53 45 58    osses.Record\SEX
				50 5C 31 5C 64 69 73 63   6F 6E 6E 65 63 74 73 00    P\1\disconnects.
				52 65 63 6F 72 64 5C 53   45 58 50 5C 31 5C 72 61    Record\SEXP\1\ra
				74 69 6E 67 00 52 65 63   6F 72 64 5C 53 45 58 50    ting.Record\SEXP
				5C 31 5C 68 69 67 68 20   72 61 74 69 6E 67 00 52    \1\high rating.R
				65 63 6F 72 64 5C 53 45   58 50 5C 31 5C 72 61 6E    ecord\SEXP\1\ran
				6B 00 52 65 63 6F 72 64   5C 53 45 58 50 5C 31 5C    k.Record\SEXP\1\
				68 69 67 68 20 72 61 6E   6B 00 52 65 63 6F 72 64    high rank.Record
				5C 53 45 58 50 5C 31 5C   6C 61 73 74 20 67 61 6D    \SEXP\1\last gam
				65 00 52 65 63 6F 72 64   5C 53 45 58 50 5C 31 5C    e.Record\SEXP\1\
				6C 61 73 74 20 67 61 6D   65 20 72 65 73 75 6C 74    last game result
				00 00                                                ..

				FF 26 C2 01   05 00 00 00 13 00 00 00        .&..........
				EE E4 84 03 6E 73 6C 40   63 6C 6F 75 64 00 63 6C    ....nsl@cloud.cl
				6F 75 64 00 67 75 65 73   74 00 48 65 72 6E 40 73    oud.guest.Hern@s
				65 65 6D 65 00 6F 72 69   6F 6E 40 00 70 72 6F 66    eeme.orion@.prof
				69 6C 65 5C 73 65 78 00   70 72 6F 66 69 6C 65 5C    ile\sex.profile\
				61 67 65 00 70 72 6F 66   69 6C 65 5C 6C 6F 63 61    age.profile\loca
				74 69 6F 6E 00 70 72 6F   66 69 6C 65 5C 64 65 73    tion.profile\des
				63 72 69 70 74 69 6F 6E   00 52 65 63 6F 72 64 5C    cription.Record\
				53 74 61 72 5C 30 5C 77   69 6E 73 00 52 65 63 6F    Star\0\wins.Reco
				72 64 5C 53 74 61 72 5C   30 5C 6C 6F 73 73 65 73    rd\Star\0\losses
				00 52 65 63 6F 72 64 5C   53 74 61 72 5C 30 5C 64    .Record\Star\0\d
				69 73 63 6F 6E 6E 65 63   74 73 00 52 65 63 6F 72    isconnects.Recor
				64 5C 53 74 61 72 5C 30   5C 6C 61 73 74 20 67 61    d\Star\0\last ga
				6D 65 00 52 65 63 6F 72   64 5C 53 74 61 72 5C 30    me.Record\Star\0
				5C 6C 61 73 74 20 67 61   6D 65 20 72 65 73 75 6C    \last game resul
				74 00 52 65 63 6F 72 64   5C 53 74 61 72 5C 31 5C    t.Record\Star\1\
				77 69 6E 73 00 52 65 63   6F 72 64 5C 53 74 61 72    wins.Record\Star
				5C 31 5C 6C 6F 73 73 65   73 00 52 65 63 6F 72 64    \1\losses.Record
				5C 53 74 61 72 5C 31 5C   64 69 73 63 6F 6E 6E 65    \Star\1\disconne
				63 74 73 00 52 65 63 6F   72 64 5C 53 74 61 72 5C    cts.Record\Star\
				31 5C 72 61 74 69 6E 67   00 52 65 63 6F 72 64 5C    1\rating.Record\
				53 74 61 72 5C 31 5C 68   69 67 68 20 72 61 74 69    Star\1\high rati
				6E 67 00 52 65 63 6F 72   64 5C 53 74 61 72 5C 31    ng.Record\Star\1
				5C 72 61 6E 6B 00 52 65   63 6F 72 64 5C 53 74 61    \rank.Record\Sta
				72 5C 31 5C 68 69 67 68   20 72 61 6E 6B 00 52 65    r\1\high rank.Re
				63 6F 72 64 5C 53 74 61   72 5C 31 5C 6C 61 73 74    cord\Star\1\last
				20 67 61 6D 65 00 52 65   63 6F 72 64 5C 53 74 61     game.Record\Sta
				72 5C 31 5C 6C 61 73 74   20 67 61 6D 65 20 72 65    r\1\last game re
				73 75 6C 74 00 00                                    sult..
				*/
#define CLIENT_STATSREQ 0x26ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        name_count;
		bn_int        key_count;
		bn_int        requestid; /* 78 52 82 02 */
		/* player name */
		/* field key ... */
	} PACKED_ATTR() t_client_statsreq;
#define CLIENT_STATSREQ_UNKNOWN1 0x02825278
	/******************************************************/


	/******************************************************/
	/*
							  FF 26 23 00 01 00 00 00            .&#.....
							  13 00 00 00 78 52 82 02   00 00 00 00 00 00 00 00    ....xR..........
							  00 00 00 00 00 00 00 00   00 00 00 00 00 00          ..............

							  FF 26 13 02   05 00 00 00 13 00 00 00        .&..........
							  EE E4 84 03 20 20 A1 F0   00 20 20 A1 F0 00 68 74    ....  ...  ...ht
							  74 70 3A 2F 2F 6E 73 6C   2E 6B 6B 69 72 69 2E 6F    tp://nsl.kkiri.o
							  72 67 00 20 20 20 20 20   20 20 20 20 20 20 A2 CB    rg.           ..
							  20 50 72 6F 74 6F 73 73   20 69 73 20 54 68 65 20     Protoss is The
							  42 65 73 74 20 A2 CB 20   0D 0A 0D 0A 20 20 20 49    Best .. ....   I
							  66 20 59 6F 75 20 57 61   6E 74 20 54 6F 20 4B 6E    f You Want To Kn
							  6F 77 20 41 62 6F 75 74   20 55 73 2C 20 47 6F 0D    ow About Us, Go.
							  0A 0D 0A 20 20 20 20 20   20 20 20 20 20 20 20 20    ...
							  20 68 74 74 70 3A 2F 2F   6E 73 6C 2E 6B 6B 69 72     http://nsl.kkir
							  69 2E 6F 72 67 00 38 38   00 37 30 00 33 00 32 39    i.org.88.70.3.29
							  32 36 30 33 31 30 20 33   39 36 31 36 37 38 32 34    260310 396167824
							  30 00 4C 4F 53 53 00 30   00 30 00 30 00 30 00 00    0.LOSS.0.0.0.0..
							  00 00 00 00 00 6D 00 31   35 00 53 69 6E 67 61 70    .....m.15.Singap
							  6F 72 65 20 00 00 32 37   00 31 00 36 00 32 39 32    ore ..27.1.6.292
							  35 34 32 33 37 20 32 34   35 37 32 33 30 39 38 00    54237 245723098.
							  44 52 41 57 00 30 00 30   00 30 00 30 00 00 00 00    DRAW.0.0.0.0....
							  00 00 00 00 00 00 00 00   00 00 00 00 00 00 00 00    ................
							  00 00 00 00 00 00 00 00   00 00 31 00 00 00 32 39    ..........1...29
							  32 35 39 38 31 32 20 31   31 35 33 38 33 37 39 34    259812 115383794
							  36 00 57 49 4E 00 30 00   30 00 30 00 30 00 00 00    6.WIN.0.0.0.0...
							  00 00 00 00 B0 C5 BD C3   B1 E2 00 3F 3F 00 B1 D9    ...........??...
							  B0 C5 C1 F6 20 BE F8 C0   BD 2E 00 BA B0 C0 DA B8    .... ...........
							  AE 20 6F 72 69 6F 6E 20   2C 2C 20 C3 CA C4 DA C6    . orion ,, .....
							  C4 C0 CC B0 A1 20 BE C6   B3 E0 BF EB 2E 2E 0D 0A    ..... ..........
							  C4 ED C4 ED 2E 2E 0D 0A   0D 0A C1 B9 B6 F3 20 C0    .............. .
							  DF C7 CF B4 C2 20 B3 D1   20 3A 20 6E 73 6C B3 D1    ..... .. : nsl..
							  B5 E9 0D 0A C0 DF C7 CF   B4 C2 20 C7 C1 C5 E4 20    .......... ....
							  3A 20 6E 73 6C 40 74 6F   74 6F 72 6F 00 35 31 00    : nsl@totoro.51.
							  34 38 00 36 00 32 39 32   35 39 32 36 33 20 39 35    48.6.29259263 95
							  36 35 30 35 39 30 32 00   4C 4F 53 53 00 36 00 36    6505902.LOSS.6.6
							  00 32 00 39 39 30 00 31   30 32 37 00 00 00 32 39    .2.990.1027...29
							  32 35 39 32 35 38 20 33   31 32 32 37 39 38 38 32    259258 312279882
							  00 4C 4F 53 53 00 00                                 .LOSS..
							  */
#define SERVER_STATSREPLY 0x26ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        name_count;
		bn_int        key_count;
		bn_int        requestid; /* 78 52 82 02 */ /* EE E4 84 03 */ /* same as request */
		/* field values ... */
	} PACKED_ATTR() t_server_statsreply;
	/******************************************************/


	/******************************************************/
	/*
	FF 29 25 00 CF 17 28 00   A3 D3 2C 5C F4 18 02 40    .)%...(...,\...@
	F9 B8 EA F4 A5 B1 3F 39   85 89 2D DB 18 2D B9 D4    ......?9..-..-..
	52 6F 73 73 00                                       Ross.
	*/
#define CLIENT_LOGINREQ1 0x29ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        ticks;
		bn_int        sessionkey;
		bn_int        password_hash2[5]; /* hash of ticks, key, and hash1 */
		/* player name */
	} PACKED_ATTR() t_client_loginreq1;
	/******************************************************/


	/******************************************************/
#define SERVER_LOGINREPLY1 0x29ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        message;
	} PACKED_ATTR() t_server_loginreply1;
#define SERVER_LOGINREPLY1_MESSAGE_FAIL    0x00000000
#define SERVER_LOGINREPLY1_MESSAGE_SUCCESS 0x00000001
	/******************************************************/


	/******************************************************/
	/*
	FF 31 3B 00 22 1A 9A 00   64 B7 C5 21 2C 82 57 F4    .1;."...d..!,.W.
	0A 36 73 25 1E A5 42 5F   FA 36 54 97 BC 65 3F E1    .6s%..B_.6T..e?.
	7D 5A 54 17 4C 33 B9 1A   09 25 49 45 99 52 69 45    }ZT.L3...%IE.RiE
	B1 E6 5C 9C 77 72 73 70   6F 69 00                   ..\.wrspoi.

	FF 31 3B 00 3A 5B 9B 00   64 B7 C5 21 99 32 14 B7    .1;.:[..d..!.2..
	89 02 3C 28 4A 75 84 05   70 EF B5 A7 99 CA 7E 12    ..<(Ju..p.....~.
	7D 5A 54 17 4C 33 B9 1A   09 25 49 45 99 52 69 45    }ZT.L3...%IE.RiE
	B1 E6 5C 9C 77 72 73 70   6F 69 00                   ..\.wrspoi.

	from D2 LoD 1.08
	FF 31 38 00 79 8E 09 00   F5 1C EB 4E 2E 7A 9C 6A    .18.y......N.z.j
	13 43 4A 49 2C CE 49 24   2E 65 FB 95 44 FC C3 B2    .CJI,.I$.e..D...
	E1 75 1A DA 19 36 EE 9B   AA EE 23 99 F0 82 4F F8    .u...6....#...O.
	B9 6B 09 55 62 6F 62 00                              .k.Ubob.
	*/
#define CLIENT_CHANGEPASSREQ 0x31ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        ticks; /* FIXME: upper two bytes seem constant for each client? */
		bn_int        sessionkey;
		bn_int        oldpassword_hash2[5]; /* hash of ticks, key, hash1 */
		bn_int        newpassword_hash1[5]; /* hash of lowercase password w/o null */
		/* player name */
	} PACKED_ATTR() t_client_changepassreq;
	/******************************************************/


	/******************************************************/
	/*
	FF 31 08 00 01 00 00 00                              .1......

	FF 31 08 00 00 00 00 00                              .1......
	*/
#define SERVER_CHANGEPASSACK 0x31ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        message;
	} PACKED_ATTR() t_server_changepassack;
#define SERVER_CHANGEPASSACK_MESSAGE_FAIL    0x00000000
#define SERVER_CHANGEPASSACK_MESSAGE_SUCCESS 0x00000001
	/******************************************************/


	/******************************************************/
	/*
							 FF 0A 0F 00 0F                   .....
							 4D 79 41 63 63 6F 75 6E          MyAccoun
							 74 00 00                                             t..

							 ff 0a 1f 00 4c 69 66 65   6c 69 6b 65 00 42 65 74    ....Lifelike.Bet
							 61 57 65 73 74 2c 4c 69   66 65 6c 69 6b 65 00       aWest,Lifelike.

							 Diablo II 1.03 - amazon just after creation
							 ff 0a 20 00 47 6f   64 64 65 73 73 00 51 61      .. .Goddess.Qa
							 72 61 74 68 52 65 61 6c   6d 2c 47 6f 64 64 65 73    rathRealm,Goddes
							 73 00                                                s.
							 */
#define CLIENT_PLAYERINFOREQ 0x0aff
	typedef struct
	{
		t_bnet_header h;
		/* player name */
		/* player info */ /* used by Diablo and D2 (character,Realm) */
	} PACKED_ATTR() t_client_playerinforeq;
	/******************************************************/


	/******************************************************/
	/*
							  FF 0A 29 00 4D 79 41 63            ..).MyAc
							  63 6F 75 6E 74 00 50 58   45 53 20 30 20 30 20 30    count.PXES 0 0 0
							  20 30 20 30 20 30 00 4D   79 41 63 63 6F 75 6E 74     0 0 0.MyAccount
							  00                                                   .

							  FF 0A 2F 00 6C 61 77 75   65 66 00 4C 54 52 44 20    ../.lawuef.LTRD
							  31 20 30 20 30 20 33 30   20 31 30 20 32 30 20 32    1 0 0 30 10 20 2
							  35 20 31 30 30 20 30 00   6C 61 77 75 65 66 00       5 100 0.lawuef.

							  ff 0a 53 00 4d 6f 4e   6b 32 6b 00 56 44 32 44     ..S.MoNk2k.VD2D
							  42 65 74 61 57 65 73 74   2c 4d 6f 4e 6b 2d 65 65    BetaWest,MoNk-ee
							  2c 87 80 06 01 01 01 01   ff 4c ff 02 02 ff ff ff    ,........L......
							  ff ff ff 01 ff 48 48 48   48 ff a6 ff 48 48 ff ff    .....HHHH...HH..
							  ff ff ff ff 0f 88 80 80   80 ff ff ff 00 4d 6f 4e    .............MoN
							  6b 32 6b 00                                          k2k.

							  ff 0a 5c 00 47 61 6d 65   6f 66 4c 69 66 65 00 56    ..\.GameofLife.V
							  44 32 44 42 65 74 61 57   65 73 74 2c 4c 69 66 65    D2DBetaWest,Life
							  6c 69 6b 65 2c 87 80 01   01 01 01 01 ff ff ff 01    like,...........
							  01 ff ff ff ff ff ff 03   ff ff ff ff ff ff ff ff    ................
							  ff ff ff ff ff ff ff ff   01 80 80 ff ff ff 80 80    ................
							  00 47 61 6d 65 6f 66 4c   69 66 65 00 ff 0f 3c 00    .GameofLife...<.
							  07 00 00 00 21 00 00 00   7d 00 00 00 00 00 00 00    ....!...}.......
							  d8 94 f6 08 55 9e 77 02   47 61 6d 65 6f 66 4c 69    ....U.w.GameofLi
							  66 65 00 44 69 61 62 6c   6f 20 49 49 20 42 65 74    fe.Diablo II Bet
							  61 57 65 73 74 2d 31 00   ff 0f 68 00 01 00 00 00    aWest-1...h.....
							  00 00 00 00 10 00 00 00   00 00 00 00 d8 94 f6 08    ................
							  b1 65 77 02 65 76 69 6c   67 72 75 73 73 6c 65 72    .ew.evilgrussler
							  00 56 44 32 44 42 65 74   61 57 65 73 74 2c 74 61    .VD2DBetaWest,ta
							  72 61 6e 2c 83 80 ff ff   ff ff ff 2f ff ff ff ff    ran,......./....
							  ff ff ff ff ff ff 03 ff   ff ff ff ff ff ff ff ff    ................
							  ff ff ff ff ff ff ff 07   80 80 80 80 ff ff ff 00    ................

							  ff 0a 59 00 47 61 6d 65   6f 66 4c 69 66 65 00 56    ..Y.GameofLife.V
							  44 32 44 42 65 74 61 57   65 73 74 2c 42 4e 45 54    D2DBetaWest,BNET
							  44 2c 87 80 01 01 01 01   01 ff ff ff 01 01 ff ff    D,..............
							  ff ff ff ff 02 ff ff ff   ff ff ff ff ff ff ff ff    ................
							  ff ff ff ff ff 01 80 80   ff ff ff 80 80 00 47 61    ..............Ga
							  6d 65 6f 66 4c 69 66 65   00                         meofLife.
							  */
#define SERVER_PLAYERINFOREPLY 0x0aff
	typedef struct
	{
		t_bnet_header h;
		/* player name */
		/* status */
		/* player name?! (maybe character name?) */
	} PACKED_ATTR() t_server_playerinforeply;
	/*
	 * status string:
	 *
	 * for STAR, SEXP, SSHR < 1.10:
	 * "%s %u %u %u %u %u"
	 *  client tag (RATS, PXES, RHSS)
	 *  rating
	 *  number (ladder rank)
	 *  stars  (normal wins)
	 *  unknown3 (always zero?)
	 *  unknown4 (always zero?) FIXME: I don't see this last one in any dumps...
	 is this only a SEXP thing?

	 * for STAR, SEXP, SSHR >= 1.10:
	 * "%s %u %u %u %u %u %u %u %u %u %s"
	 *  client tag (RATS, PXES, RHSS)
	 *  rating
	 *  number (ladder rank)
	 *  stars  (normal wins)
	 *  spawned (1 of spawned, 0 otherwise)
	 *  unknown4 (always zero?)
	 *  highest ladder rating
	 *  unknown6 (always zero?)
	 *  unknown7 (always zero?)
	 *  icon tag (usually client tag)


	 *
	 * for DRTL:
	 * "%s %u %u %u %u %u %u %u %u %u"
	 *  client tag (LTRD)
	 *  level
	 *  class (0==warrior, 1==rogue, 2==sorcerer)
	 *  dots (times killed diablo)
	 *  strength
	 *  magic
	 *  dexterity
	 *  vitality
	 *  gold
	 *  unknown2 (always zero?)

	 *
	 * for D2DV:
	 * "%s%s,%s,"
	 * client tag (VD2D)
	 * realm
	 * character name
	 * 43 unknown bytes
	 */
#define PLAYERINFO_DRTL_CLASS_WARRIOR  0
#define PLAYERINFO_DRTL_CLASS_ROGUE    1
#define PLAYERINFO_DRTL_CLASS_SORCERER 2
	/******************************************************/


	/******************************************************/
#define CLIENT_PROGIDENT2 0x0bff
	typedef struct
	{
		t_bnet_header h;
		bn_int        clienttag;
	} PACKED_ATTR() t_client_progident2;
	/******************************************************/


	/******************************************************/
#define CLIENT_JOINCHANNEL 0x0cff
	typedef struct
	{
		t_bnet_header h;
		bn_int        channelflag;
	} PACKED_ATTR() t_client_joinchannel;
#define CLIENT_JOINCHANNEL_NORMAL  0x00000000
#define CLIENT_JOINCHANNEL_GENERIC 0x00000001
#define CLIENT_JOINCHANNEL_CREATE  0x00000002
	/******************************************************/


	/******************************************************/
#define SERVER_CHANNELLIST 0x0bff
	typedef struct
	{
		t_bnet_header h;
		/* channel names */
	} PACKED_ATTR() t_server_channellist;
	/******************************************************/


	/******************************************************/
	/*
	We don't use this for now. It makes the client put
	the list of IPs/hostnames into the registry.

	FF 04 8F 00 00 00 00 00   32 30 39 2E 36 37 2E 31    ........209.67.1
	33 36 2E 31 37 34 3B 32   30 37 2E 36 39 2E 31 39    36.174;207.69.19
	34 2E 32 31 30 3B 32 30   37 2E 36 39 2E 31 39 34    4.210;207.69.194
	2E 31 38 39 3B 32 31 36   2E 33 32 2E 37 33 2E 31    .189;216.32.73.1
	37 34 3B 32 30 39 2E 36   37 2E 31 33 36 2E 31 37    74;209.67.136.17
	31 3B 32 30 36 2E 37 39   2E 32 35 34 2E 31 39 32    1;206.79.254.192
	3B 32 30 37 2E 31 33 38   2E 33 34 2E 33 3B 32 30    ;207.138.34.3;20
	39 2E 36 37 2E 31 33 36   2E 31 37 32 3B 65 78 6F    9.67.136.172;exo
	64 75 73 2E 62 61 74 74   6C 65 2E 6E 65 74 00       dus.battle.net.
	*/
#define SERVER_SERVERLIST 0x04ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        unknown1; /* 00 00 00 00 */
		/* list */
	} PACKED_ATTR() t_server_serverlist;
#define SERVER_SERVERLIST_UNKNOWN1 0x00000000
	/******************************************************/


	/******************************************************/
	/*
	FF 0F 30 00 01 00 00 00   00 00 00 00 00 00 00 00    ..0.............
	00 00 00 00 00 00 00 00   00 00 00 00 52 6F 73 73    ............Ross
	00 52 41 54 53 20 30 20   30 20 30 20 30 20 30 00    .RATS 0 0 0 0 0.

	FF 0F 38 00 07 00 00 00   21 00 00 00 64 00 00 00    ..8.....!...d...
	00 00 00 00 D8 94 F6 07   B3 2C 6E 02 4D 6F 4E 6B    .........,n.MoNk
	32 6B 00 44 69 61 62 6C   6F 20 49 49 20 42 65 74    2k.Diablo II Bet
	61 57 65 73 74 2D 31 00                              aWest-1.

	MT_ADD:
	0x0000: ff 0a 53 00 4d 6f 4e 6b   32 6b 00 56 44 32 44 42    ..S.MoNk2k.VD2DB
	0x0010: 65 74 61 57 65 73 74 2c   4d 6f 4e 6b 2d 65 65 2c    etaWest,MoNk-ee,
	0x0020: 83 80 06 01 01 01 01 ff   4c ff 02 02 ff ff ff ff    ........L.......
	0x0030: ff ff 01 ff 48 48 48 48   ff a6 ff 48 48 ff ff ff    ....HHHH...HH...
	0x0040: ff ff ff 10 80 80 80 80   ff ff ff 00 4d 6f 4e 6b    ............MoNk
	0x0050: 32 6b 00 ff 0f 38 00 07   00 00 00 21 00 00 00 6d    2k...8.....!...m
	0x0060: 00 00 00 00 00 00 00 d8   94 f6 08 a4 46 6e 02 4d    ............Fn.M
	0x0070: 6f 4e 6b 32 6b 00 44 69   61 62 6c 6f 20 49 49 20    oNk2k.Diablo II
	0x0080: 42 65 74 61 57 65 73 74   2d 31 00 ff 0f 62 00 01    BetaWest-1...b..
	0x0090: 00 00 00 00 00 00 00 28   00 00 00 00 00 00 00 d8    .......(........
	0x00a0: 94 f6 07 69 fb 6d 02 4e   6f 72 62 62 6f 00 56 44    ...i.m.Norbbo.VD
	0x00b0: 32 44 42 65 74 61 57 65   73 74 2c 44 6f 6f 73 68    2DBetaWest,Doosh
	0x00c0: 2c 87 80 05 02 01 01 01   2b ff 1b 02 02 ff ff ff    ,.......+.......
	0x00d0: ff ff ff 03 ff ff ff ff   ff ff ff ff ff ff ff ff    ................
	0x00e0: ff ff ff ff 0a 80 80 80   80 ff ff ff 00 ff 0f 65    ...............e
	0x00f0: 00 01 00 00 00 00 00 00   00 32 00 00 00 00 00 00    .........2......
	0x0100: 00 d8 94 f6 07 23 00 6e   02 6e 6a 67 6f 61 6c 69    .....#.n.njgoali
	0x0110: 65 00 56 44 32 44 42 65   74 61 57 65 73 74 2c 73    e.VD2DBetaWest,s
	0x0120: 68 65 69 6b 61 2c 83 80   05 02 01 02 02 ff 4c ff    heika,........L.
	0x0130: 02 02 ff ff ff ff ff ff   02 ff ff ff ff ff ff ff    ................
	0x0140: ff ff ff ff ff ff ff ff   ff 09 80 80 80 80 ff ff    ................
	0x0150: ff 00 ff 0f 67 00 01 00   00 00 00 00 00 00 1f 00    ....g...........
	0x0160: 00 00 00 00 00 00 d8 94   f6 09 eb 24 6e 02 72 6f    ...........$n.ro
	0x0170: 62 6d 6d 73 64 00 56 44   32 44 42 65 74 61 57 65    bmmsd.VD2DBetaWe
	0x0180: 73 74 2c 56 61 6e 63 6f   75 76 65 72 2c 83 80 05    st,Vancouver,...
	0x0190: 02 02 01 01 30 ff 1b 02   02 ff ff ff ff ff ff 04    ....0...........
	0x01a0: 4f ff ff ff ff ff ff a9   ff ff ff ff ff ff ff ff    O...............
	0x01b0: 09 80 80 80 80 ff ff ff   00 ff 0f 62 00 01 00 00    ...........b....
	0x01c0: 00 00 00 00 00 6e 00 00   00 00 00 00 00 ce 4f fe    .....n........O.
	0x01d0: c0 f9 02 15 01 4c 79 63   74 68 69 73 00 56 44 32    .....Lycthis.VD2
	0x01e0: 44 42 65 74 61 57 65 73   74 2c 45 6c 6c 65 2c 83    DBetaWest,Elle,.
	0x01f0: 80 04 02 01 01 01 ff 4d   ff 02 02 ff ff ff ff ff    .......M........
	0x0200: ff 01 ff ff ff ff ff ff   ff ff ff ff ff ff ff ff    ................
	0x0210: ff ff 06 80 80 80 80 ff   ff ff 00 ff 0f 6b 00 01    .............k..
	0x0220: 00 00 00 00 00 00 00 19   01 00 00 00 00 00 00 ce    ................
	0x0230: 4f fe c1 e2 1b 9c 00 52   6f 62 4d 69 74 63 68 65    O......RobMitche
	0x0240: 6c 6c 00 56 44 32 44 42   65 74 61 57 65 73 74 2c    ll.VD2DBetaWest,
	0x0250: 53 6f 6f 6e 65 72 64 65   64 2c 83 80 06 02 02 01    Soonerded,......
	0x0260: 01 46 46 ff 02 02 ff ff   ff ff ff ff 05 ff ff ff    .FF.............
	0x0270: ff ff ff 29 ff ff ff ff   ff ff ff ff ff 11 80 82    ...)............
	0x0280: 80 80 ff ff ff 00 ff 0f   66 00 01 00 00 00 00 00    ........f.......
	0x0290: 00 00 bc 00 00 00 00 00   00 00 d8 94 f6 09 8f 3f    ...............?
	0x02a0: 6e 02 4d 61 72 6c 6f 63   6b 31 00 56 44 32 44 42    n.Marlock1.VD2DB
	0x02b0: 65 74 61 57 65 73 74 2c   6d 61 72 6c 6f 63 6b 2c    etaWest,marlock,
	0x02c0: 83 80 ff ff ff ff ff ff   ff ff ff ff ff ff ff ff    ................
	0x02d0: ff ff 05 ff ff ff ff ff   ff ff ff ff ff ff ff ff    ................
	0x02e0: ff ff ff 01 80 80 80 80   ff ff ff 00 ff 0f 63 00    ..............c.
	0x02f0: 01 00 00 00 00 00 00 00   c8 00 00 00 00 00 00 00    ................
	0x0300: d1 43 88 aa bd ce 3c 00   42 2d 57 61 74 74 7a 00    .C....<.B-Wattz.

	From bnetd-0.4.23pre18 to Diablo II 1.03
	FF 0F 69 00 09 00  00 00 00 00 00 00 12 00   I@..i... ........
	00 00 00 00 00 00 00 00  00 00 00 00 00 00 45 6C   ........ ......El
	66 6C 6F 72 64 00 56 44  32 44 51 61 72 61 74 68   flord.VD 2DQarath
	52 65 61 6C 6D 2C 46 61  6B 65 43 68 61 72 2C 83   Realm,Fa keChar,.
	80 FF FF FF FF FF 2F FF  FF FF FF FF FF FF FF FF   ....../. ........
	FF 03 FF FF FF FF FF FF  FF FF FF FF FF FF FF FF   ........ ........
	FF FF 07 80 80 80 80 FF  FF FF 00                  ........ ...
	*/
#define SERVER_MESSAGE 0x0fff
	typedef struct
	{
		t_bnet_header h;
		bn_int        type;
		bn_int        flags;     /* player flags (or channel flags for MT_CHANNEL) */
		bn_int        latency;
		bn_int        player_ip;  /* always zero? */
		bn_int        account_num; /* player's IP (big endian), no longer used, always 0D F0 AD BA */
		bn_int        reg_auth;  /* server ip and/or reg auth? CD key and/or account number? */
		/* player name */
		/* text */
	} PACKED_ATTR() t_server_message;
#define SERVER_MESSAGE_PLAYER_IP_DUMMY 0x00000000
	/* nok */
#define SERVER_MESSAGE_REG_AUTH 0xBAADF00D /* 0D F0 AD BA */
	/* #define SERVER_MESSAGE_REG_AUTH 0x07f694d8 */
#define SERVER_MESSAGE_ACCOUNT_NUM 0x0df0adba
	/* For MT_ADD, MT_JOIN, the text portion looks like:
	 *
	 * for STAR, SEXP, SSHR:
	 * "%4c %u %u %u %u %u"
	 *  client tag (RATS, PXES, RHSS)
	 *  rating
	 *  number (ladder rank)
	 *  stars  (normal wins)
	 *  unknown3 (always zero?)
	 *  unknown4 (always zero?)
	 *
	 * for DRTL:
	 * "%4c %u %u %u %u %u %u %u %u %u"
	 *  client tag (LTRD) FIXME: RHSD?
	 *  level
	 *  class (0==warrior, 1==rogue, 2==sorcerer)
	 *  dots (times killed diablo)
	 *  strength
	 *  magic
	 *  dexterity
	 *  vitality
	 *  gold
	 *  unknown2 (always zero?)
	 *
	 * for CHAT:
	 * FIXME: ???  "%4c"
	 * client tag (TAHC)
	 *
	 * FIXME: Warcraft II?
	 *
	 * for D2DV:
	 * open:
	 * "%4c"
	 * client tag (VD2D)
	 * closed:
	 * "%4c%s,%s,%s"
	 * client tag (VD2D)
	 * realm name
	 * character name
	 * character info
	 */
#define SERVER_MESSAGE_TYPE_ADDUSER             0x00000001 /* ADD,USER,SHOWUSER */
#define SERVER_MESSAGE_TYPE_JOIN                0x00000002
#define SERVER_MESSAGE_TYPE_PART                0x00000003 /* LEAVE */
#define SERVER_MESSAGE_TYPE_WHISPER             0x00000004
#define SERVER_MESSAGE_TYPE_TALK                0x00000005 /* MESSAGE */
#define SERVER_MESSAGE_TYPE_BROADCAST           0x00000006
#define SERVER_MESSAGE_TYPE_CHANNEL             0x00000007 /* JOINING */
	/* unused?                                      0x00000008 */
#define SERVER_MESSAGE_TYPE_USERFLAGS           0x00000009
#define SERVER_MESSAGE_TYPE_WHISPERACK          0x0000000a /* WHISPERSENT */
	/* unused?                                      0x0000000b */
	/* unused?                                      0x0000000c */
#define SERVER_MESSAGE_TYPE_CHANNELFULL         0x0000000d
#define SERVER_MESSAGE_TYPE_CHANNELDOESNOTEXIST 0x0000000e
#define SERVER_MESSAGE_TYPE_CHANNELRESTRICTED   0x0000000f
	/* unused?                                      0x00000010 */
	/* unused?                                      0x00000011 */
#define SERVER_MESSAGE_TYPE_INFO                0x00000012
#define SERVER_MESSAGE_TYPE_ERROR               0x00000013
	/* unused?                                      0x00000014 */
	/* unused?                                      0x00000015 */
	/* unused?                                      0x00000016 */
#define SERVER_MESSAGE_TYPE_EMOTE               0x00000017

	/****** Player Flags ******/
	/* flag bits for above struct */

	/* ADDED BY UNDYING SOULZZ 4/7/02 */
#define W3_ICON_SET					0x00000000
#define MAX_STR_RACELEN				20
#define MAX_STR_ACCTPASSLEN         10
#define W3_RACE_RANDOM				32
#define W3_RACE_HUMANS				1
#define W3_RACE_ORCS				2
#define W3_RACE_UNDEAD				8
#define W3_RACE_NIGHTELVES			4
#define W3_RACE_DEMONS				16

#define W3_ICON_RANDOM				0 /* - Although when client presses random in PG and it sends "32" its "0" for icon */
#define W3_ICON_HUMANS				1
#define W3_ICON_ORCS				2
#define W3_ICON_UNDEAD				3 /* - Although when client presses undead in PG and it sends "8" its "3" for icon */
#define W3_ICON_NIGHTELVES			4
#define W3_ICON_DEMONS				5

	/* Icon setup  3RAW then <accounts level> <Race> <race wins> */
	/* Races: 1 = human, 2 = orc, 8 = undead, 4 = nightelf  32 = random */
	/* Misc icons are 6-9 * - There might be some icons not defined*/
	/* If you find them let us know pse,  forums.cheatlist.com in War3 Hacking/Development */

	/*Human Icons*/
#define W3_ICON_HUMAN_FOOTMAN					"3RAW 1 1 11"
#define W3_ICON_HUMAN_KNIGHT					"3RAW 1 1 100"
#define W3_ICON_HUMAN_ARCHMAGE					"3RAW 1 1 250"
#define W3_ICON_HUMAN_HERO						"3RAW 1 1 1000"

	/*Orc Icons*/
#define W3_ICON_ORC_PEON						"3RAW 1 2 00" /* default icon , unless u change it in code*/
#define W3_ICON_ORC_GRUNT						"3RAW 1 2 10"
#define W3_ICON_ORC_TAUREN						"3RAW 1 2 100"
#define W3_ICON_ORC_FARSEER						"3RAW 1 2 250"
#define W3_ICON_ORC_HERO						"3RAW 1 2 1000"

	/*Undead Icons*/
#define W3_ICON_UNDEAD_GHOUL					"3RAW 1 3 10"
#define W3_ICON_UNDEAD_ABOM						"3RAW 1 3 100"
#define W3_ICON_UNDEAD_LICH						"3RAW 1 3 250"
#define W3_ICON_UNDEAD_HERO						"3RAW 1 3 1000"

	/*Night Elf Icons*/
#define W3_ICON_ELF_ARCHER						"3RAW 1 4 10"
#define W3_ICON_ELF_DRUIDCLAW					"3RAW 1 4 100"
#define W3_ICON_ELF_PRIESMOON					"3RAW 1 4 250"
#define W3_ICON_ELF_HERO						"3RAW 1 4 1000"

	/*Random Icons , like NPC's, Creeps*/
#define W3_ICON_RANDOM_GREENDRAGON				"3RAW 1 9 10"
#define W3_ICON_RANDOM_BLACKDRAGON				"3RAW 1 9 100"
#define W3_ICON_RANDOM_REDDRAGON				"3RAW 1 9 250"
#define W3_ICON_RANDOM_BLUEDRAGON				"3RAW 1 9 1000"

	/* End of Undying Edits */

	/*      Blizzard Entertainment employee */
#define MF_BLIZZARD 0x00000001 /* blue Blizzard logo */
	/*      Channel operator */
#define MF_GAVEL    0x00000002 /* gavel */
	/*      Speaker in moderated channel */
#define MF_VOICE    0x00000004 /* megaphone */
	/*      System operator */
#define MF_BNET     0x00000008 /* (old: blue Blizzard, new: green b.net) or red BNETD logo */
	/*      Chat bot or other user without UDP support */
#define MF_PLUG     0x00000010 /* tiny plug to right of icon, no UDP */
	/*      Squelched/Ignored user */
#define MF_X        0x00000020 /* big red X */
	/*      Special guest of Blizzard Entertainment */
#define MF_SHADES   0x00000040 /* sunglasses */
	/* unused           0x00000080 */
	/*      Use BEL character in error codes. Some bots use it as a flag. Battle.net */
	/*      stopped supporting it recently. */
#define MF_BEEP     0x00000100 /* no change in icon */
	/*      Registered Professional Gamers League player */
#define MF_PGLPLAY  0x00000200 /* PGL player logo */
	/*      Registered Professional Gamers League official */
#define MF_PGLOFFL  0x00000400 /* PGL official logo */
	/*      Registered KBK player */
#define MF_KBKPLAY  0x00000800 /* KBK player logo */
	/*      Official KBK Referee */
#define MF_KBKREF   0x00001000 /* KBK referee logo */ /* FIXME: this number may be wrong */
	/* unused... FIXME: how many bits work? 16 or 32? */

	/****** Channel Flags ******/
	/* flag bits for MT_CHANNEL message */
#define CF_PUBLIC     0x00000001 /* public channel */
#define CF_MODERATED  0x00000002 /* moderated channel */
#define CF_RESTRICTED 0x00000004 /* ? restricted channel ? */
#define CF_THEVOID    0x00000008 /* "The Void" */
#define CF_SYSTEM     0x00000020 /* system channel */
#define CF_OFFICIAL   0x00001000 /* official channel */
	/*
	 * Examples:
	 * 0x00001003 Blizzard Tech Support
	 * 0x00001001 Open Tech Support
	 * 0x00000021 Diablo II USA-1  or  War2BNE USA-1
	 * 0x0000000D warez
	 * 0x00000009 The Void
	 * 0x00000001 War2 Ladder Challenges  or  Diablo II PvP
	 * 0x00000000 clan randomchannel
	 * 0x00000000 randomchannel
	 */
	/******************************************************/


	/******************************************************/
#define CLIENT_MESSAGE 0x0eff
	typedef struct
	{
		t_bnet_header h;
		/* text */
	} PACKED_ATTR() t_client_message;
	/******************************************************/


	/******************************************************/
	/*
	FF 09 17 00 03 00 00 00   FF FF 00 00 00 00 00 00    ................
	19 00 00 00 00 00 00                                 .......

	FF 09 24 00 00 00 00 00   00 00 00 00 00 00 00 00    ..$.............
	01 00 00 00 4C 61 64 64   65 72 20 31 20 6F 6E 20    ....Ladder 1 on
	31 00 00 00                                          1...
	*/
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
	/*
	FF 09 35 00 01 00 00 00   00 00 00 00 03 00 01 00    ..5.............
	00 00 00 00 02 00 17 E0   80 7B 4F 0D 00 00 00 00    .........{O.....
	00 00 00 00 04 00 00 00   64 00 00 00 4D 79 47 61    ........d...MyGa
	6D 65 00 00 00                                       me...

	FF 09 5B 00 01 00 00 00   00 00 00 00 03 00 01 00    ..[.............
	02 00 17 E0 80 7B 4F 0D   00 00 00 00 00 00 00 00    .....{O.........
	04 00 00 00 2B 00 00 00   47 61 6D 65 00 50 61 73    ....+...Game.Pas
	73 00 2C 33 34 2C 31 32   2C 35 2C 31 2C 33 2C 31    s.,34,12,5,1,3,1
	2C 63 63 63 33 36 34 30   36 2C 2C 42 6F 62 0D 43    ,ccc36406,,Bob.C
	68 61 6C 6C 65 6E 67 65   72 0D 00                   hallenger..

	FF 09 D4 03 0A 00 00 00   0C 00 00 00 09 04 00 00    ................
	02 00 17 E0 CD E8 B5 E1   00 00 00 00 00 00 00 00    ................
	00 00 00 00 3C 00 00 00   4A 65 73 73 65 27 73 20    ....<...Jesse's
	57 6F 72 6C 64 00 00 32   0D 4C 69 7A 7A 69 65 2E    World..2.Lizzie.
	42 6F 72 64 65 6E 0D 4C   54 52 44 20 34 30 20 31    Borden.LTRD 40 1
	20 33 20 31 32 31 20 31   32 36 20 33 30 36 20 31     3 121 126 306 1
	33 36 20 35 34 37 30 32   20 30 00                   ...

	FF 09 70 00 01 00 00 00   0F 00 04 00 09 04 00 00    ..p.............
	02 00 17 E0 C6 0B 13 3C   00 00 00 00 00 00 00 00    .......<........
	04 00 00 00 C5 00 00 00   4C 61 64 64 65 72 20 31    ........Ladder 1
	20 6F 6E 20 31 00 00 2C   2C 2C 36 2C 32 2C 66 2C     on 1..,,,6,2,f,
	34 2C 66 63 63 35 38 65   34 61 2C 37 32 30 30 2C    4,fcc58e4a,7200,
	49 63 65 36 39 62 75 72   67 0D 46 6F 72 65 73 74    Ice69burg.Forest
	20 54 72 61 69 6C 20 42   4E 45 2E 70 75 64 0D 00     Trail BNE.pud..

	# war3
	# 66 packet from server: type=0x09ff(SERVER_GAMELISTREPLY) length=131 class=bnet
	0000:   FF 09 83 00 01 00 00 00   01 00 00 00 09 04 00 00    ................
	0010:   02 00 17 E0 18 CF BF 9B   00 00 00 00 00 00 00 00    ................
	0020:   10 00 00 00 0F 00 00 00   33 20 6F 6E 20 33 20 64    ........3 on 3 d
	0030:   61 72 6B 20 66 6F 72 65   73 74 00 00 35 31 30 30    ark forest..5100
	0040:   30 30 30 30 30 01 03 01   01 81 01 81 01 73 27 25    00000........s'%
	0050:   15 29 4D 61 71 53 73 5D   63 65 75 61 5D A9 29 37    .)MaqSs]ceua].)7
	0060:   29 45 61 73 6B 69 21 47   6F 73 65 73 75 DD 2F 77    )Easki!Gosesu./w
	0070:   33 6D 01 4B 61 D7 69 73   69 69 6F 5B 53 07 4B 5D    3m.Ka.isiio[S.K]
	0080:   01 01 00                                             ...

	[23:32] <@nok-> 0000:   FF 09 E9 01 04 00 00 00   01 00 00 00 09 04 00 00    ................
	[23:32] <@nok-> 0010:   02 00 17 E0 40 69 1B 07   00 00 00 00 00 00 00 00    ....@i..........

	0000:   FF 09 7A 01 03 00 00 00   01 00 00 00 09 04 00 00    ..z.............
	0010:   02 00 17 E0 18 2C 7E 7B   00 00 00 00 00 00 00 00    .....,~{........
	0020:   10 00 00 00 09 00 00 00   34 20 6F 6E 20 34 20 4D    ........4 on 4 M
	0030:   69 73 74 00 00 37 31 30   30 30 30 30 30 30 01 03    ist..710000000..
	0040:   01 01 89 01 89 01 75 4D   7B 27 A1 4D 61 71 53 73    ......uM{'.MaqSs
	0050:   5D 63 65 75 61 5D B9 29   39 29 47 6F 6D 65 17 6D    ]ceua].)9)Gome.m
	0060:   73 21 69 6F 21 75 75 69   65 21 4D 69 73 75 1D 2F    s!io!uuie!Misu./
	0070:   77 33 6D 01 51 73 BB 69   6F 63 65 2D 4D 75 17 63    w3m.Qs.ioce-Mu.c
	0080:   69 67 65 73 01 01 00 01   00 00 00 09 04 00 00 02    iges............

	*/
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


	/******************************************************/

} /* namespace pvpgn */

#endif /* INCLUDED_BNET_PROTOCOL_CHAT_H */
