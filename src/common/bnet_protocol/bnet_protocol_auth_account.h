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

/* Account management packets: PASSCHANGEREQ/REPLY, PASSCHANGEPROOFREQ/REPLY, */
/*   CREATEACCOUNT_W3                                                          */
/* Split from bnet_protocol_auth.h (plan 05 §3 / SOLID-S)                     */
#ifndef INCLUDED_BNET_PROTOCOL_AUTH_ACCOUNT_H
#define INCLUDED_BNET_PROTOCOL_AUTH_ACCOUNT_H

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

} /* namespace pvpgn */

#endif /* INCLUDED_BNET_PROTOCOL_AUTH_ACCOUNT_H */
