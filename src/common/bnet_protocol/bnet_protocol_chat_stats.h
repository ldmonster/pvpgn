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

/* Chat player-stats/profile packets: STATSREQ, STATSREPLY, PLAYERINFOREQ, PLAYERINFOREPLY */
#ifndef INCLUDED_BNET_PROTOCOL_CHAT_STATS_H
#define INCLUDED_BNET_PROTOCOL_CHAT_STATS_H

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
#define CLIENT_STATSREQ 0x26ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        name_count;
		bn_int        key_count;
		bn_int        requestid; /* 78 52 82 02 */
		/* player name */
		/* field key ... */
	} PACKED_ATTR() t_client_statsreq;
#define CLIENT_STATSREQ_UNKNOWN1 0x02825278
	/******************************************************/


	/******************************************************/
#define SERVER_STATSREPLY 0x26ff
	typedef struct
	{
		t_bnet_header h;
		bn_int        name_count;
		bn_int        key_count;
		bn_int        requestid; /* 78 52 82 02 */ /* EE E4 84 03 */ /* same as request */
		/* field values ... */
	} PACKED_ATTR() t_server_statsreply;
	/******************************************************/


	/******************************************************/
#define CLIENT_PLAYERINFOREQ 0x0aff
	typedef struct
	{
		t_bnet_header h;
		/* player name */
		/* player info */ /* used by Diablo and D2 (character,Realm) */
	} PACKED_ATTR() t_client_playerinforeq;
	/******************************************************/


	/******************************************************/
#define SERVER_PLAYERINFOREPLY 0x0aff
	typedef struct
	{
		t_bnet_header h;
		/* player name */
		/* status */
		/* player name?! (maybe character name?) */
	} PACKED_ATTR() t_server_playerinforeply;
	/*
	 * status string:
	 *
	 * for STAR, SEXP, SSHR < 1.10:
	 * "%s %u %u %u %u %u"
	 *  client tag (RATS, PXES, RHSS)
	 *  rating
	 *  number (ladder rank)
	 *  stars  (normal wins)
	 *  unknown3 (always zero?)
	 *  unknown4 (always zero?) FIXME: I don't see this last one in any dumps...
	 is this only a SEXP thing?

	 * for STAR, SEXP, SSHR >= 1.10:
	 * "%s %u %u %u %u %u %u %u %u %u %s"
	 *  client tag (RATS, PXES, RHSS)
	 *  rating
	 *  number (ladder rank)
	 *  stars  (normal wins)
	 *  spawned (1 of spawned, 0 otherwise)
	 *  unknown4 (always zero?)
	 *  highest ladder rating
	 *  unknown6 (always zero?)
	 *  unknown7 (always zero?)
	 *  icon tag (usually client tag)

	 *
	 * for DRTL:
	 * "%s %u %u %u %u %u %u %u %u %u"
	 *  client tag (LTRD)
	 *  level
	 *  class (0==warrior, 1==rogue, 2==sorcerer)
	 *  dots (times killed diablo)
	 *  strength
	 *  magic
	 *  dexterity
	 *  vitality
	 *  gold
	 *  unknown2 (always zero?)

	 *
	 * for D2DV:
	 * "%s%s,%s,"
	 * client tag (VD2D)
	 * realm
	 * character name
	 * 43 unknown bytes
	 */
#define PLAYERINFO_DRTL_CLASS_WARRIOR  0
#define PLAYERINFO_DRTL_CLASS_ROGUE    1
#define PLAYERINFO_DRTL_CLASS_SORCERER 2
	/******************************************************/

} /* namespace pvpgn */

#endif /* INCLUDED_BNET_PROTOCOL_CHAT_STATS_H */
