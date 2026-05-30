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

/* CD-key validation packets: CDKEY, CDKEY2, CDKEY3, CDKEYREPLY/2/3 */
/* Split from bnet_protocol_auth.h (plan 05 §3 / SOLID-S)           */
#ifndef INCLUDED_BNET_PROTOCOL_AUTH_CDKEY_H
#define INCLUDED_BNET_PROTOCOL_AUTH_CDKEY_H

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
	/*
	FF 30 0E 00 00 00 00 00   63 6C 6F 75 64 00            .0......cloud.
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

} /* namespace pvpgn */

#endif /* INCLUDED_BNET_PROTOCOL_AUTH_CDKEY_H */
