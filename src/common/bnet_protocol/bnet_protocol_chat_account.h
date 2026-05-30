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

/* Chat account-management packets: CHANGEGAMEPORT, CREATEACCOUNT_W3, CREATEACCTREQ2 */
#ifndef INCLUDED_BNET_PROTOCOL_CHAT_ACCOUNT_H
#define INCLUDED_BNET_PROTOCOL_CHAT_ACCOUNT_H

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

} /* namespace pvpgn */

#endif /* INCLUDED_BNET_PROTOCOL_CHAT_ACCOUNT_H */
