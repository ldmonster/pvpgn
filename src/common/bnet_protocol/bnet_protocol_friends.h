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

/* Friends/ArrangedTeam packets: ARRANGEDTEAM_ACCEPT_INVITE */
#ifndef INCLUDED_BNET_PROTOCOL_FRIENDS_H
#define INCLUDED_BNET_PROTOCOL_FRIENDS_H

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

#define CLIENT_ARRANGEDTEAM_ACCEPT_INVITE 0xfdff
	typedef struct
	{
		t_bnet_header h;
	} PACKED_ATTR() t_client_arrangedteam_accept_invite;

	/* clan handling */

#define SERVER_CLAN_MEMBER_CHIEFTAIN 0x04
#define SERVER_CLAN_MEMBER_SHAMAN 0x03
#define SERVER_CLAN_MEMBER_GRUNT 0x02
#define SERVER_CLAN_MEMBER_PEON 0x01
#define SERVER_CLAN_MEMBER_NEW 0x00
#define SERVER_CLAN_MEMBER_OFFLINE 0x00
#define SERVER_CLAN_MEMBER_ONLINE 0x01
#define SERVER_CLAN_MEMBER_CHANNEL 0x02
#define SERVER_CLAN_MEMBER_GAME 0x03
#define SERVER_CLAN_MEMBER_PRIVATE_GAME 0x04

	/*Paquet #267
	0x0000   FF 75 0A 00 00 00 42 54-54 04                     u....BTT.
	*/
	/*
	300: recv class=bnet[0x02] type=unknown[0x70ff] length=12
	0000:   FF 70 0C 00 01 00 00 00   00 64 73 66                .p.......dsf
	300: send class=bnet[0x02] type=unknown[0x70ff] length=56
	0000:   FF 70 38 00 01 00 00 00   00 09 44 4A 50 32 00 44    .p8.......DJP2.D
	0010:   4A 50 33 00 44 4A 50 34   00 44 4A 50 35 00 44 4A    JP3.DJP4.DJP5.DJ
	0020:   50 36 00 44 4A 50 37 00   44 4A 50 38 00 44 4A 50    P6.DJP7.DJP8.DJP
	0030:   39 00 44 4A 50 31 30 00                              9.DJP10.
	300: recv class=bnet[0x02] type=CLIENT_FRIENDINFOREQ[0x66ff] length=5
	0000:   FF 66 05 00 01                                       .f...
	300: send class=bnet[0x02] type=unknown[0x66ff] length=12
	0000:   FF 66 0C 00 01 00 00 00   00 00 00 00                .f..........
	300: recv class=bnet[0x02] type=unknown[0x71ff] length=64
	0000:   FF 71 40 00 01 00 00 00   74 65 73 74 00 00 64 73    .q@.....test..ds
	0010:   66 09 44 4A 50 32 00 44   4A 50 33 00 44 4A 50 34    f.DJP2.DJP3.DJP4
	0020:   00 44 4A 50 35 00 44 4A   50 36 00 44 4A 50 37 00    .DJP5.DJP6.DJP7.
	0030:   44 4A 50 38 00 44 4A 50   39 00 44 4A 50 31 30 00    DJP8.DJP9.DJP10.
	300: send class=bnet[0x02] type=unknown[0x71ff] length=10
	0000:   FF 71 0A 00 01 00 00 00   00 00                      .q........
	300: send class=bnet[0x02] type=unknown[0x75ff] length=12
	0000:   FF 75 0C 00 00 00 00 00   64 73 66 00                .u......dsf.
	300: recv class=bnet[0x02] type=CLIENT_FRIENDINFOREQ[0x66ff] length=5
	0000:   FF 66 05 00 02                                       .f...
	300: send class=bnet[0x02] type=unknown[0x66ff] length=12
	0000:   FF 66 0C 00 02 00 00 00   00 00 00 00                .f..........
	*/

} /* namespace pvpgn */

#endif /* INCLUDED_BNET_PROTOCOL_FRIENDS_H */
