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

/* Chat file-transfer packets: UDPOK, FILEINFOREQ, FILEINFOREPLY */
#ifndef INCLUDED_BNET_PROTOCOL_CHAT_FILE_H
#define INCLUDED_BNET_PROTOCOL_CHAT_FILE_H

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

} /* namespace pvpgn */

#endif /* INCLUDED_BNET_PROTOCOL_CHAT_FILE_H */
