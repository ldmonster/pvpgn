/*
 * Copyright (C) 2001  Ross Combs (rocombs@cs.nmsu.edu)
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


/*****/
#ifndef JUST_NEED_TYPES
#ifndef INCLUDED_HANDLE_TELNET_PROTOS
#define INCLUDED_HANDLE_TELNET_PROTOS

// R203: implementation relocated to
// `src/v3/integration/legacy_bnetd/src/handle_telnet_link.cpp`.
// This header keeps the public declaration so callers in
// `src/bnetd/server.cpp` can still `#include "handle_telnet.h"`.

#define JUST_NEED_TYPES
#include "connection.h"
#include "common/packet.h"
#undef JUST_NEED_TYPES

namespace pvpgn
{

	namespace bnetd
	{

		extern int handle_telnet_packet(t_connection * c, t_packet const * const packet);

	}

}

#endif
#endif
