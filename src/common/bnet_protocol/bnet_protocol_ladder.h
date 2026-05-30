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

/* Misc2 packets: MESSAGEBOX, REQUIREDWORK, EXTRAWORK */
#ifndef INCLUDED_BNET_PROTOCOL_LADDER_H
#define INCLUDED_BNET_PROTOCOL_LADDER_H

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

		 */
	} PACKED_ATTR() t_server_clanmemberupdate;



#define SERVER_MESSAGEBOX 0x19ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        style;
		/* Text */
		/* Caption */
	} PACKED_ATTR() t_server_messagebox;
#define SERVER_MESSAGEBOX_OK    0x00000000
#define SERVER_MESSAGEBOX_OKCANCEL 0x00000001
#define SERVER_MESSAGEBOX_YESNO 0x00000004

#define SERVER_REQUIREDWORK 0x4Cff
	typedef struct
	{
		t_bnet_header h;
		/* FileName */
	} PACKED_ATTR() t_server_requiredwork;

#define CLIENT_EXTRAWORK 0x4bff
	typedef struct
	{
		t_bnet_header h;
		bn_short        gametype;
		bn_short        length;
		/* Data */
	} PACKED_ATTR() t_client_extrawork;

} /* namespace pvpgn */

#endif /* INCLUDED_BNET_PROTOCOL_LADDER_H */
