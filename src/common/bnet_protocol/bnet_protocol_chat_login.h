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

/* Chat login/password packets: CDKEYREPLY2, LOGINREQ1, LOGINREPLY1, CHANGEPASSREQ, CHANGEPASSACK */
#ifndef INCLUDED_BNET_PROTOCOL_CHAT_LOGIN_H
#define INCLUDED_BNET_PROTOCOL_CHAT_LOGIN_H

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
	FF 36 0C 00 01 00 00 00   52 6F 62 00                .6......Rob.
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
	/*
	FF 31 3B 00 22 1A 9A 00   64 B7 C5 21 2C 82 57 F4    .1;."...d..!,.W.
	...
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

} /* namespace pvpgn */

#endif /* INCLUDED_BNET_PROTOCOL_CHAT_LOGIN_H */
