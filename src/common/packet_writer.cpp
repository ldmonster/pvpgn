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
// packet_writer.cpp — packet write/append operations (SOLID-S split from packet.cpp)
// Responsibility: appending strings and binary data into a packet buffer
#include "common/setup_before.h"
#include "common/packet.h"

#include <cstring>

#include "common/eventlog.h"
#include "common/field_sizes.h"
#include "common/lstr.h"
#include "common/setup_after.h"


namespace pvpgn
{

	extern int packet_append_string(t_packet * packet, char const * str)
	{
		unsigned int   len;
		unsigned short addlen;
		unsigned short size;

		if (!packet)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got NULL packet");
			return -1;
		}
		if (!str)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got NULL string");
			return -1;
		}

		len = std::strlen(str) + 1;
		size = packet_get_size(packet);
		if (size >= MAX_PACKET_SIZE)
			return -1;

		if (MAX_PACKET_SIZE - (unsigned int)size > len)
			addlen = len;
		else
			addlen = MAX_PACKET_SIZE - size;
		if (addlen < 1)
			return -1;

		std::memcpy(packet->u.data + size, str, addlen - 1);
		packet->u.data[size + addlen - 1] = '\0';
		packet_set_size(packet, size + addlen);

		return (int)addlen;
	}


	extern int packet_append_ntstring(t_packet * packet, char const * str)
	{
		unsigned int   len;
		unsigned short addlen;
		unsigned short size;

		if (!packet)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got NULL packet");
			return -1;
		}
		if (!str)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got NULL string");
			return -1;
		}

		len = std::strlen(str);
		size = packet_get_size(packet);
		if (size >= MAX_PACKET_SIZE)
			return -1;

		if (MAX_PACKET_SIZE - (unsigned int)size > len)
			addlen = len;
		else
			addlen = MAX_PACKET_SIZE - size;
		if (addlen < 1)
			return -1;

		std::memcpy(packet->u.data + size, str, addlen);
		packet_set_size(packet, size + addlen);

		return (int)addlen;
	}


	extern int packet_append_lstr(t_packet * packet, t_lstr *lstr)
	{
		unsigned short addlen;
		unsigned short size;

		if (!packet)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got NULL packet");
			return -1;
		}
		if (!lstr || !lstr_get_str(lstr))
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got NULL string");
			return -1;
		}

		size = packet_get_size(packet);
		if (size >= MAX_PACKET_SIZE)
			return -1;

		if (MAX_PACKET_SIZE - (unsigned int)size > lstr_get_len(lstr))
			addlen = lstr_get_len(lstr);
		else
			addlen = MAX_PACKET_SIZE - size;
		if (addlen < 1)
			return -1;

		std::memcpy(packet->u.data + size, lstr_get_str(lstr), addlen - 1);
		packet->u.data[size + addlen - 1] = '\0';
		packet_set_size(packet, size + addlen);

		return (int)addlen;
	}


	extern int packet_append_data(t_packet * packet, void const * data, unsigned int len)
	{
		unsigned short addlen;
		unsigned short size;

		if (!packet)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got NULL packet");
			return -1;
		}
		if (!data)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got NULL data");
			return -1;
		}

		size = packet_get_size(packet);
		if (size >= MAX_PACKET_SIZE)
			return -1;

		if (MAX_PACKET_SIZE - (unsigned int)size > len)
			addlen = len;
		else
			addlen = MAX_PACKET_SIZE - size;
		if (addlen < 1)
			return -1;

		std::memcpy(packet->u.data + size, data, addlen);
		packet_set_size(packet, size + addlen);

		return (int)addlen;
	}

} // namespace pvpgn
