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

/* Realm-list packets: REALMLISTREQ, REALMLISTREQ_110, REALMLISTREPLY, REALMLISTREPLY_110 */
/* Split from bnet_protocol_auth.h (plan 05 §3 / SOLID-S)                                */
#ifndef INCLUDED_BNET_PROTOCOL_AUTH_REALM_H
#define INCLUDED_BNET_PROTOCOL_AUTH_REALM_H

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

} /* namespace pvpgn */

#endif /* INCLUDED_BNET_PROTOCOL_AUTH_REALM_H */
