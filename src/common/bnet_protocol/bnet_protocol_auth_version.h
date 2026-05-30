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
// bnet_protocol_auth_version.h — version-check and auth-request packets
// (plan 15 §3 / SOLID-S split from bnet_protocol_auth.h)
// Covers: CREATEACCTREQ1, UNKNOWN_2B, PROGIDENT, AUTHREQ1/109, AUTHREPLY1/109,
//         REGSNOOPREQ/REPLY, ICONREQ/REPLY, LADDERSEARCHREQ/REPLY
#ifndef INCLUDED_BNET_PROTOCOL_AUTH_VERSION_H
#define INCLUDED_BNET_PROTOCOL_AUTH_VERSION_H

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
	/******************************************************/


	/******************************************************/
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
#define CLIENT_REGSNOOPREPLY 0x18ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        unknown1; /* 00 00 00 00 */ /* same as request? */
		/* registry value (string, dword, or binary */
	} PACKED_ATTR() t_client_regsnoopreply;
	/******************************************************/


	/******************************************************/
#define CLIENT_ICONREQ 0x2dff
	typedef struct
	{
		t_bnet_header h;
	} PACKED_ATTR() t_client_iconreq;
	/******************************************************/


	/******************************************************/
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

} /* namespace pvpgn */

#endif /* INCLUDED_BNET_PROTOCOL_AUTH_VERSION_H */
