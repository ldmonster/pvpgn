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

/* Login-flow packets: PROFILEREQ/REPLY, UNKNOWN_37/39, LOGINREQ2,        */
/*   MOTD_W3, LOGINREQ_W3, LOGINREPLY_W3, LOGONPROOFREQ/REPLY, t_d2char_info */
/* Split from bnet_protocol_auth.h (plan 05 §3 / SOLID-S)                 */
#ifndef INCLUDED_BNET_PROTOCOL_AUTH_LOGIN_H
#define INCLUDED_BNET_PROTOCOL_AUTH_LOGIN_H

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
	01 2B FF 1B 02 02 FF FF   FF FF FF FF 03 FF FF FF    .+.............
	FF FF FF FF FF FF FF FF   A8 FF FF FF FF FF FF FF    ................
	FF 10 80 82 80 80 FF FF   FF 00 42 65 74 61 57 65    ..........BetaWe
	73 74 2C 4D 6F 4E 6B 2D   65 65 00 83 80 06 01 01    st,MoNk-ee......
	01 01 FF 4C FF 02 02 FF   FF FF FF FF FF 01 FF 48    ...L...........H
	48 48 48 FF A6 FF 48 48   FF FF FF FF FF FF 0F 80    HHH...HH........
	80 80 80 FF FF FF 00 42   65 74 61 57 65 73 74 2C    .......BetaWest,
	4D 6F 4E 6B 2D 74 77 6F   00 87 80 01 01 01 01 01    MoNk-two........
	FF FF FF 01 01 FF FF FF   FF FF FF 02 FF FF FF FF    ................
	FF FF FF FF FF FF FF FF   FF FF FF FF 01 84 80 FF    ................
	FF FF 80 80 00                                       .....

	^-- 1: (BetaWest) MoNk
	2: (BetaWest) MoNk-e
	3: (BetaWest) MoNk-ee
	4: (BetaWest) MoNk-two
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

} /* namespace pvpgn */

#endif /* INCLUDED_BNET_PROTOCOL_AUTH_LOGIN_H */
