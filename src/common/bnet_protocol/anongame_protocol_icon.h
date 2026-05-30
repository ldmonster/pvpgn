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

/*
 * anongame_protocol_icon.h — option 09: icon request/reply
 *
 * Covers 0x44ff option 09 (get icon) and option 0A (set icon).
 * The client request uses the base t_client_anongame struct (option byte only).
 *
 * Structs:
 *   t_server_findanongame_iconreply  — server icon table reply
 */

#ifndef INCLUDED_ANONGAME_PROTOCOL_ICON
#define INCLUDED_ANONGAME_PROTOCOL_ICON

#ifdef JUST_NEED_TYPES
# include "common/bn_type.h"
#else
# define JUST_NEED_TYPES
# include "common/bn_type.h"
# undef JUST_NEED_TYPES
#endif

namespace pvpgn
{

	/***********************************************************************************/
	/* option 9 - icon request */
#define SERVER_FINDANONGAME_ICONREPLY           0x44ff
	typedef struct{
		t_bnet_header         h;
		bn_byte               option;                 /* as received from client */
		bn_int                count;                  /* as received from client */
		bn_int                curricon;               /* current icon code */
		bn_byte               table_width;            /* the icon table width */
		bn_byte               table_size;             /* the icon table total size */
		/* table data */
	} PACKED_ATTR() t_server_findanongame_iconreply;

}

#endif /* INCLUDED_ANONGAME_PROTOCOL_ICON */
