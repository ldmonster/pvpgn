/*
 * Copyright (C) 1999,2000,2001  Ross Combs (rocombs@cs.nmsu.edu)
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
// Network address (CIDR) creation, destruction, formatting, and containment check.

#include "addr_internal.h"

namespace pvpgn
{

	static char const * netaddr_num_to_addr_str(unsigned int netipaddr, unsigned int netmask)
	{
		static unsigned int curr = 0;
		static char         temp[4][64];
		struct sockaddr_in  tsa;

		curr = (curr + 1) % 4;

		std::memset(&tsa, 0, sizeof(tsa));
		tsa.sin_family = AF_INET;
		tsa.sin_port = htons(static_cast<unsigned short>(0));
		tsa.sin_addr.s_addr = htonl(netipaddr);

		char addrstr[INET_ADDRSTRLEN] = { 0 };
		inet_ntop(AF_INET, &(tsa.sin_addr), addrstr, sizeof(addrstr));
		std::sprintf(temp[curr], "%.32s/0x%08x", addrstr, netmask);

		return temp[curr];
	}


	extern t_netaddr * netaddr_create_str(char const * netstr)
	{
		t_netaddr *  netaddr;
		char const * netipstr;
		char const * netmaskstr;
		unsigned int netip;
		unsigned int netmask;

		if (!netstr)
		{
			LOG_ERROR(__FUNCTION__, "unable to allocate memory for netaddr");
			return NULL;
		}

		std::string temp_storage = netstr;
		char* temp = temp_storage.empty() ? nullptr : &temp_storage[0];
		if (!(netipstr = std::strtok(temp, "/")))
		{
			return NULL;
		}
		if (!(netmaskstr = std::strtok(NULL, "/")))
		{
			return NULL;
		}

		netaddr = new t_netaddr{};

		/* FIXME: call getnetbyname() first, then host_lookup() */
		if (!host_lookup(netipstr, &netip))
		{
			LOG_ERROR(__FUNCTION__, "could not lookup net");
			delete netaddr;
			return NULL;
		}
		netaddr->ip = netip;

		if (str_to_uint(netmaskstr, &netmask) < 0)
		{
			struct sockaddr_in tsa;
			
			if (inet_pton(AF_INET, netmaskstr, &tsa.sin_addr) == 1)
				netmask = ntohl(tsa.sin_addr.s_addr);
			else
			{
				LOG_ERROR(__FUNCTION__, "could not convert mask");
				delete netaddr;
				return NULL;
			}
		}
		else
		{
			if (netmask > 32)
			{
				LOG_ERROR(__FUNCTION__, "network bits must be less than or equal to 32 ({})", netmask);
				delete netaddr;
				return NULL;
			}
			/* for example, 8 -> 11111111000000000000000000000000 */
			if (netmask != 0)
				netmask = ~((1u << (32 - netmask)) - 1u);
		}
		netaddr->mask = netmask;

		return netaddr;
	}


	extern int netaddr_destroy(t_netaddr const * netaddr)
	{
		if (!netaddr)
		{
			LOG_ERROR(__FUNCTION__, "got NULL netaddr");
			return -1;
		}

		delete const_cast<t_netaddr*>(netaddr); /* avoid warning */

		return 0;
	}


	extern char * netaddr_get_addr_str(t_netaddr const * netaddr, char * str, unsigned int len)
	{
		if (!netaddr)
		{
			LOG_ERROR(__FUNCTION__, "got NULL netaddr");
			return NULL;
		}
		if (!str)
		{
			LOG_ERROR(__FUNCTION__, "got NULL str");
			return NULL;
		}
		if (len < 2)
		{
			LOG_ERROR(__FUNCTION__, "str too short");
			return NULL;
		}

		std::strncpy(str, netaddr_num_to_addr_str(netaddr->ip, netaddr->mask), len - 1); /* FIXME: format nicely with x.x.x.x/bitcount */
		str[len - 1] = '\0';

		return str;
	}


	extern int netaddr_contains_addr_num(t_netaddr const * netaddr, unsigned int ipaddr)
	{
		if (!netaddr)
		{
			LOG_ERROR(__FUNCTION__, "got NULL netaddr");
			return -1;
		}

		return (ipaddr&netaddr->mask) == netaddr->ip;
	}

} // namespace pvpgn
