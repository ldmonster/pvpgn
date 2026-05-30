/*
 * Copyright (C) 1998,1999,2000  Ross Combs (rocombs@cs.nmsu.edu)
 * Copyright (C) 1999,2000,2001  Marco Ziech (mmz@gmx.net)
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
// packet_reader.cpp — packet read/query operations (SOLID-S split from packet.cpp)
// Responsibility: extracting strings and binary data from a received packet buffer
#include "common/setup_before.h"
#include "common/packet.h"

#include "common/eventlog.h"
#include "common/field_sizes.h"
#include "common/setup_after.h"


namespace pvpgn
{

	extern void const * packet_get_raw_data_const(t_packet const * packet, unsigned int offset)
	{
		unsigned int size;

		if (!packet)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got NULL packet");
			return NULL;
		}
		size = (unsigned int)packet_get_size(packet);
		if (offset >= size || (((offset >= MAX_PACKET_SIZE) && (packet->pclass != packet_class_wolgameres)) || (offset >= MAX_WOL_GAMERES_PACKET_SIZE)))
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got bad offset {} for packet size {}", offset, size);
			return NULL;
		}

		return packet->u.data + offset;
	}


	extern void * packet_get_raw_data(t_packet * packet, unsigned int offset)
	{
		unsigned int size;

		if (!packet)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got NULL packet");
			return NULL;
		}
		size = (unsigned int)packet_get_size(packet);
		if (offset >= size || (((offset >= MAX_PACKET_SIZE) && (packet->pclass != packet_class_wolgameres)) || (offset >= MAX_WOL_GAMERES_PACKET_SIZE)))
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got bad offset {} for packet size {}", offset, size);
			return NULL;
		}

		return packet->u.data + offset;
	}


	extern void * packet_get_raw_data_build(t_packet * packet, unsigned int offset)
	{
		if (!packet)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got NULL packet");
			return NULL;
		}

		if (((offset >= MAX_PACKET_SIZE) && (packet->pclass != packet_class_wolgameres)) || (offset >= MAX_WOL_GAMERES_PACKET_SIZE))
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got bad offset {} for packet", offset);
			return NULL;
		}

		return packet->u.data + offset;
	}


	/* maxlen includes room for NUL char */
	extern char const * packet_get_str_const(t_packet const * packet, unsigned int offset, unsigned int maxlen)
	{
		unsigned int size;
		unsigned int pos;

		if (!packet)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got NULL packet");
			return NULL;
		}
		size = (unsigned int)packet_get_size(packet);
		if (offset >= size)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got bad offset {} for packet size {}", offset, size);
			return NULL;
		}

		for (pos = offset; packet->u.data[pos] != '\0'; pos++)
		if (pos >= size || pos - offset > maxlen)
			return NULL;
		if (pos >= size || pos - offset > maxlen) /* NUL must be inside too */
			return NULL;
		return packet->u.data + offset;
	}


	extern void const * packet_get_data_const(t_packet const * packet, unsigned int offset, unsigned int len)
	{
		unsigned int size;

		if (!packet)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got NULL packet");
			return NULL;
		}
		if (len<1)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got zero length");
			return NULL;
		}
		size = (unsigned int)packet_get_size(packet);
		if (offset + len>size)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got bad offset {} and length {} for packet size {}", offset, len, size);
			return NULL;
		}

		return packet->u.data + offset;
	}

} // namespace pvpgn
