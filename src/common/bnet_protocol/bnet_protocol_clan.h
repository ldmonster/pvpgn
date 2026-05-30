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

/* Clan packets: CLAN_CREATE, CLAN_INVITE, CLANMEMBER_*, etc. */
#ifndef INCLUDED_BNET_PROTOCOL_CLAN_H
#define INCLUDED_BNET_PROTOCOL_CLAN_H

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

#define CLIENT_CLAN_CREATEREQ 0x70ff
	typedef struct{
		t_bnet_header        h;
		bn_int               count;
		bn_int               clantag;
	} PACKED_ATTR() t_client_clan_createreq;


#define SERVER_CLAN_CREATEREPLY 0x70ff
	typedef struct{
		t_bnet_header        h;
		bn_int               count;
		bn_byte			   check_result;
		bn_byte			   friend_count;
		/* player name in chan or mutual
		char player_name[sizeof (friend_name)]; */
	} PACKED_ATTR() t_server_clan_createreply;
#define SERVER_CLAN_CREATEREPLY_CHECK_OK 0x00
#define SERVER_CLAN_CREATEREPLY_CHECK_ALLREADY_IN_USE 0x01
#define SERVER_CLAN_CREATEREPLY_CHECK_TIME_LIMIT 0x02
#define SERVER_CLAN_CREATEREPLY_CHECK_EXCEPTION 0x04
#define SERVER_CLAN_CREATEREPLY_CHECK_INVALID_CLAN_TAG 0x0a

	/*
	3852: recv class=bnet[0x02] type=unknown[0x71ff] length=70
	0000:   FF 71 46 00 01 00 00 00   53 75 62 57 61 72 5A 6F    .qF.....SubWarZo
	0010:   6E 65 00 00 5A 57 53 09   44 4A 50 32 00 44 4A 50    ne..ZWS.DJP2.DJP
	0020:   33 00 44 4A 50 34 00 44   4A 50 35 00 44 4A 50 36    3.DJP4.DJP5.DJP6
	0030:   00 44 4A 50 37 00 44 4A   50 38 00 44 4A 50 31 30    .DJP7.DJP8.DJP10
	0040:   00 44 4A 50 39 00                                    .DJP9.
	*/
#define CLIENT_CLAN_CREATEINVITEREQ 0x71ff
	typedef struct{
		t_bnet_header        h;
		bn_int               count;
		/* Clan Name (\0 terminated string)
		   bn_int			    clantag;
		   bn_byte			friend_count; //Number of friend selected
		   Name of friend (\0 terminated string)
		   */
	} PACKED_ATTR() t_client_clan_createinvitereq;

	/*3756: send class=bnet[0x02] type=unknown[0x71ff] length=14
	0000:   FF 71 0E 00 02 00 00 00   05 44 4A 50 32 00          .q.......DJP2.
	<- Unable to receive invitation ( PG search, already in a clan, etc... )*/
	/*3756:
	Paquet #266
	0x0000   FF 71 0A 00 05 00 00 00-00 00                     q........
	<- Clan invitation = Sucessfully done */
#define SERVER_CLAN_CREATEINVITEREPLY 0x71ff
	typedef struct{
		t_bnet_header        h;
		bn_int               count;
		bn_byte			   status; /* 0x05 = Cannot contact(not in channel screen) or already in clan | 0x04 = Decline | 0x00 = OK :)
		Name of failed member(\0 terminated string) */
	} PACKED_ATTR() t_server_clan_createinvitereply;

#define SERVER_CLAN_CREATEINVITEREQ 0x72ff
	typedef struct{
		t_bnet_header        h;
		bn_int               count;
		bn_int			   clantag;
		/* Clan Name (\0 terminated string)
		   Clan Creator (\0 terminated string)
		   bn_byte			friend_count; //Number of friend selected
		   Name of friend (\0 terminated string) */
	} PACKED_ATTR() t_server_clan_createinvitereq;

#define CLIENT_CLAN_CREATEINVITEREPLY 0x72ff
	typedef struct{
		t_bnet_header        h;
		bn_int               count;
		bn_int			   clantag;
		/* Clan Creator (\0 terminated string)
		   bn_byte			reply */ /* 0x04--decline 0x05--Cannot contact(not in channel screen) or already in clan 0x06--accept*/
	} PACKED_ATTR() t_client_clan_createinvitereply;

	/*
	3876: recv class=bnet[0x02] type=unknown[0x73ff] length=8
	0000:   FF 73 08 00 01 00 00 00                              .s......      */
#define CLIENT_CLAN_DISBANDREQ 0x73ff
	typedef struct{
		t_bnet_header        h;
		bn_int               count;
	} PACKED_ATTR() t_client_clan_disbandreq;

#define SERVER_CLAN_DISBANDREPLY 0x73ff
	typedef struct{
		t_bnet_header        h;
		bn_int               count;
		bn_byte              result;   /* 0-- Success  1-- Exception raised  2-- Clan exists less than 1 week, cannot remove */
	} PACKED_ATTR() t_server_clan_disbandreply;
#define SERVER_CLAN_DISBANDREPLY_RESULT_OK 0x0
#define SERVER_CLAN_DISBANDREPLY_RESULT_EXCEPTION 0x1
#define SERVER_CLAN_DISBANDREPLY_RESULT_FAILED 0x2

#define CLIENT_CLAN_MEMBERNEWCHIEFREQ 0x74ff
	typedef struct{
		t_bnet_header        h;
		bn_int               count;
		/*Player_Name deleted(\0 terminated) */
	} PACKED_ATTR() t_client_clan_membernewchiefreq;

#define SERVER_CLAN_MEMBERNEWCHIEFREPLY 0x74ff
	typedef struct{
		t_bnet_header        h;
		bn_int               count;
		bn_byte              result; /* 0-successful 1-failed */
	} PACKED_ATTR() t_server_clan_membernewchiefreply;
#define SERVER_CLAN_MEMBERNEWCHIEFREPLY_SUCCESS 0x00
#define SERVER_CLAN_MEMBERNEWCHIEFREPLY_FAILED 0x01

#define SERVER_CLAN_CLANACK 0x75ff
	typedef struct{
		t_bnet_header		h;
		bn_byte		unknow1; /* 0x00 */
		bn_int		clantag;
		bn_byte		status;  /* member status */
	} PACKED_ATTR() t_server_clan_clanack;

#define SERVER_CLANQUITNOTIFY 0x76ff
	typedef struct{
		t_bnet_header        h;
		bn_byte              status;
	} PACKED_ATTR() t_server_clanquitnotify;
#define SERVER_CLANQUITNOTIFY_STATUS_REMOVED_FROM_CLAN 0x01

	/*
	3876: recv class=bnet[0x02] type=unknown[0x77ff] length=13
	0000:   FF 77 0D 00 01 00 00 00   44 4A 50 31 00             .w......DJP1.  */
#define CLIENT_CLAN_INVITEREQ 0x77ff
	typedef struct{
		t_bnet_header        h;
		bn_int               count;
		/*Player_Name invited */
	} PACKED_ATTR() t_client_clan_invitereq;

#define SERVER_CLAN_INVITEREPLY 0x77ff
	typedef struct{
		t_bnet_header        h;
		bn_int               count;
		bn_byte              result;  /* 0x04--decline 0x05--Cannot contact(not in channel screen) or already in clan */
	} PACKED_ATTR() t_server_clan_invitereply;

#define CLIENT_CLANMEMBER_REMOVE_REQ 0x78ff
	typedef struct{
		t_bnet_header        h;
		bn_int               count;
		/*Player_Name deleted(\0 terminated) */
	} PACKED_ATTR() t_client_clanmember_remove_req;

#define SERVER_CLANMEMBER_REMOVE_REPLY 0x78ff
	typedef struct{
		t_bnet_header        h;
		bn_int               count;
		bn_byte              result; /* 0-successful 1-failed */
	} PACKED_ATTR() t_server_clanmember_remove_reply;
#define SERVER_CLANMEMBER_REMOVE_SUCCESS 0x00
#define SERVER_CLANMEMBER_REMOVE_FAILED 0x01

#define SERVER_CLAN_INVITEREQ 0x79ff
	typedef struct{
		t_bnet_header        h;
		bn_int               count;
		bn_int               clantag;
		/*Clan_Name (\0 terminated)
		Player_Name inviter (\0 terminated) */
	} PACKED_ATTR() t_server_clan_invitereq;

#define CLIENT_CLAN_INVITEREPLY 0x79ff
	typedef struct{
		t_bnet_header        h;
		bn_int               count;
		bn_int               clantag;
		/*Player_Name inviter (\0 terminated)
		bn_byte            reply */
	} PACKED_ATTR() t_client_clan_invitereply;

	/*
		clan codes according to bnetdocs

		0x00: Success
		0x01: In use
		0x02: Too soon
		0x03: Not enough members
		0x04: Invitation was declined
		0x05: Decline
		0x06: Accept
		0x07: Not authorized
		0x08: User not found
		0x09: Clan is full
		0x0A: Bad tag
		0x0B: Bad name
		0x0C: User not found in that clan
		*/

#define CLAN_RESPONSE_SUCCESS 		0x00
#define CLAN_RESPONSE_FAIL			0x01
#define CLAN_RESPONSE_TOO_SOON 		0x02
#define CLAN_RESPONSE_TOO_SMALL 	0x03
#define CLAN_RESPONSE_DECLINED 		0x04
#define CLAN_RESPONSE_DECLINE 		0x05
#define CLAN_RESPONSE_ACCEPT 		0x06
#define CLAN_RESPONSE_NOT_AUTHORIZED 0x07
#define CLAN_RESPONSE_NOT_FOUND 	0x08
#define CLAN_RESPONSE_CLAN_FULL 	0x09
#define CLAN_RESPONSE_BAD_TAG		0x0a
#define CLAN_RESPONSE_BAD_NAME		0x0b
#define CLAN_RESPONSE_NOT_MEMBER	0x0c

#define CLIENT_CLANMEMBER_RANKUPDATE_REQ 0x7aff
	typedef struct{
		t_bnet_header        h;
		bn_int               count;
		/*Player_Name invited(\0 terminated)
		  Player_Status(bn_byte: 1~4) */
	} PACKED_ATTR() t_client_clanmember_rankupdate_req;

#define SERVER_CLANMEMBER_RANKUPDATE_REPLY 0x7aff
	typedef struct{
		t_bnet_header        h;
		bn_int               count;
		bn_byte              result; /* 0-successful 1-failed */
	} PACKED_ATTR() t_server_clanmember_rankupdate_reply;
#define SERVER_CLANMEMBER_RANKUPDATE_SUCCESS 0x00
#define SERVER_CLANMEMBER_RANKUPDATE_FAILED 0x01

#define CLIENT_CLAN_MOTDCHG 0x7bff
	typedef struct{
		t_bnet_header        h;
		bn_int               unknow1;
		/* Motd en string ^^ */
	} PACKED_ATTR() t_client_clan_motdchg;
#define SERVER_CLAN_MOTDREPLY_UNKNOW1 0x00000000

#define SERVER_CLAN_MOTDREPLY 0x7cff
	typedef struct{
		t_bnet_header        h;
		bn_int               count;
		bn_int			   unknow1; /* 0x00000000 */
		/* MOTD */
	} PACKED_ATTR() t_server_clan_motdreply;

#define CLIENT_CLAN_MOTDREQ 0x7cff
	typedef struct{
		t_bnet_header        h;
		bn_int               count;
	} PACKED_ATTR() t_client_clan_motdreq;

	/*
	Paquet #52
	0x0000   FF 7D 10 01 01 00 00 00-13 4D 79 73 74 69 2E 53   }.......Mysti.S
	0x0010   77 5A 00 04 01 00 73 61-75 72 6F 6E 2E 73 77 7A   wZ....sauron.swz
	0x0020   00 03 00 00 73 69 6D 6F-6E 2E 53 77 5A 00 03 00   ....simon.SwZ...
	0x0030   00 4E 65 6F 2D 56 61 67-72 61 6E 74 2E 73 77 7A   .Neo-Vagrant.swz
	0x0040   00 02 00 00 77 4D 7A 00-02 00 00 6B 61 74 6E 6F   ....wMz....katno
	0x0050   6D 61 64 2E 53 77 5A 00-02 00 00 47 7A 62 65 75   mad.SwZ....Gzbeu
	0x0060   68 2E 53 77 5A 00 02 00-00 53 69 6C 76 65 72 2E   h.SwZ....Silver.
	0x0070   53 77 5A 00 02 00 00 4D-61 67 67 65 75 73 00 02   SwZ....Maggeus..
	0x0080   00 00 4F 6E 69 2D 4D 75-73 68 61 2E 53 77 5A 00   ..Oni-Musha.SwZ.
	0x0090   02 00 00 4D 61 67 67 65-75 53 2E 53 77 5A 00 02   ...MaggeuS.SwZ..
	0x00A0   00 00 53 69 72 65 5F 4C-6F 75 70 00 02 00 00 6B   ..Sire_Loup....k
	0x00B0   69 6C 6C 69 62 6F 79 00-03 00 00 52 65 64 2E 44   illiboy....Red.D
	0x00C0   72 61 4B 65 00 02 00 00-53 69 72 65 2E 53 77 5A   raKe....Sire.SwZ
	0x00D0   00 03 00 00 73 74 72 61-69 67 68 74 5F 63 6F 75   ....straight_cou
	0x00E0   67 61 72 00 02 00 00 52-65 64 44 72 61 6B 65 2E   gar....RedDrake.
	0x00F0   53 77 5A 00 02 00 00 54-72 6F 6C 6C 6F 00 02 00   SwZ....Trollo...
	0x0100   00 53 69 6C 76 65 72 62-65 61 72 64 00 00 00 00   .Silverbeard....
	*/
#define SERVER_CLANMEMBERLIST_REPLY 0x7dff
	typedef struct{
		t_bnet_header        h;
		bn_int               count;
		bn_byte			   member_count;
		/* player repeat start
		 * Name of player(\0 terminated string)
		 * bn_byte		   CHIEFTAIN = 0x04
		 *				   SHAMANS = 0x03
		 *				   GRUNT = 0x02
		 *				   PEON = 0x01
		 *				   NEW_MEMBER = 0x00 <- can't be promoted/devoted
		 * bn_byte		   online status
		 * unknown(always \0)
		 * repeat end
		 */
	} PACKED_ATTR() t_server_clanmemberlist_reply;

	/*
	Paquet #51
	0x0000   FF 7D 08 00 01 00 00 00-                          }......
	*/

#define CLIENT_CLANMEMBERLIST_REQ 0x7dff
	typedef struct{
		t_bnet_header        h;
		bn_int               count;
	} PACKED_ATTR() t_client_clanmemberlist_req;

#define SERVER_CLANMEMBER_REMOVED_NOTIFY 0x7eff
	typedef struct{
		t_bnet_header        h;
		/* Player_Name deleted(\0 terminated) */
	} PACKED_ATTR() t_server_clanmember_removed_notify;

#define SERVER_CLANMEMBERUPDATE 0x7fff
	typedef struct{
		t_bnet_header        h;
		/* Player_Name invited(\0 terminated)
		 * Player_Status(bn_byte: 1~4)
		 * Player_Online(bn_short: 0x0/0x1)

} /* namespace pvpgn */

#endif /* INCLUDED_BNET_PROTOCOL_CLAN_H */
