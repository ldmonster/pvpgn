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
// Core address creation, destruction, formatting, and accessor functions (plan 15 §3 / SOLID-S).
// Included as a sub-TU by addr.cpp — do not compile directly.

namespace pvpgn
{

#define HACK_SIZE 4

	/* both arguments are in host byte order */
	extern char const * addr_num_to_addr_str(unsigned int ipaddr, unsigned short port)
	{
		static unsigned int curr = 0;
		static char         temp[HACK_SIZE][64];
		struct sockaddr_in  tsa = { 0 };

		curr = (curr + 1) % HACK_SIZE;

		tsa.sin_family = AF_INET;
		tsa.sin_port = htons((unsigned short)0);
		tsa.sin_addr.s_addr = htonl(ipaddr);

		char addrstr[INET_ADDRSTRLEN] = { 0 };
		inet_ntop(AF_INET, &(tsa.sin_addr), addrstr, sizeof(addrstr));
		std::sprintf(temp[curr], "%.32s:%hu", addrstr, port);

		return temp[curr];
	}


	/* ipaddr is in host byte order */
	extern char const * addr_num_to_ip_str(unsigned int ipaddr)
	{
		static unsigned int curr = 0;
		static char         temp[HACK_SIZE][64];
		struct sockaddr_in  tsa;

		curr = (curr + 1) % HACK_SIZE;

		std::memset(&tsa, 0, sizeof(tsa));
		tsa.sin_family = AF_INET;
		tsa.sin_port = htons((unsigned short)0);
		tsa.sin_addr.s_addr = htonl(ipaddr);

		char addrstr[INET_ADDRSTRLEN] = { 0 };
		inet_ntop(AF_INET, &(tsa.sin_addr), addrstr, sizeof(addrstr));
		std::sprintf(temp[curr], "%.32s", addrstr);

		return temp[curr];
	}


	extern char const * host_lookup(char const * hoststr, unsigned int * ipaddr)
	{
		struct sockaddr_in tsa;
#ifdef HAVE_GETHOSTBYNAME
		struct hostent *   hp;
#endif

		if (!hoststr)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got NULL hoststr");
			return NULL;
		}
		if (!ipaddr)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got NULL ipaddr");
			return NULL;
		}

		std::memset(&tsa, 0, sizeof(tsa));
		tsa.sin_family = AF_INET;
		tsa.sin_port = htons(0);

#ifdef HAVE_GETHOSTBYNAME
		hp = gethostbyname(hoststr);
		if (!hp || !hp->h_addr_list)
#endif
		{
			if (inet_pton(AF_INET, hoststr, &tsa.sin_addr) == 1)
			{
				*ipaddr = ntohl(tsa.sin_addr.s_addr);
				return hoststr; /* We could call gethostbyaddr() on tsa to try and get the
						   official hostname but most systems would have already found
						   it when sending a dotted-quad to gethostbyname().  This is
						   good enough when that fails. */
			}
			eventlog(eventlog_level_error, __FUNCTION__, "could not lookup host \"{}\"", hoststr);
			return NULL;
		}

#ifdef HAVE_GETHOSTBYNAME
		std::memcpy(&tsa.sin_addr, (void *)hp->h_addr_list[0], sizeof(struct in_addr)); /* avoid warning */
		*ipaddr = ntohl(tsa.sin_addr.s_addr);
		if (hp->h_name)
			return hp->h_name;
		return hoststr;
#endif
	}


	extern t_addr * addr_create_num(unsigned int ipaddr, unsigned short port)
	{
		t_addr * temp;

		temp = new t_addr{};
		{
			const char* _s = addr_num_to_addr_str(ipaddr, port);
			char* _tmp = new char[std::strlen(_s) + 1];
			std::strcpy(_tmp, _s);
			temp->str = _tmp;
		}
		temp->ip = ipaddr;
		temp->port = port;
		temp->data.p = NULL;

		return temp;
	}


	extern t_addr * addr_create_str(char const * str, unsigned int defipaddr, unsigned short defport)
	{
		std::string tstr_storage;
		char *             tstr;
		t_addr *           temp;
		unsigned int       ipaddr;
		unsigned short     port;
		char const *       hoststr;
		char *             portstr;
		char const *       hostname;

		if (!str)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got NULL str");
			return NULL;
		}

		tstr_storage = str;
		tstr = tstr_storage.empty() ? nullptr : &tstr_storage[0];

		if ((portstr = std::strrchr(tstr, ':')))
		{
			char * protstr;

			*portstr = '\0';
			portstr++;

			if ((protstr = std::strrchr(portstr, '/')))
			{
				*protstr = '\0';
				protstr++;
			}

			if (portstr[0] != '\0')
			{
				if (str_to_ushort(portstr, &port) < 0)
				{
#ifdef HAVE_GETSERVBYNAME
					struct servent * sp;

					if (!(sp = getservbyname(portstr, protstr ? protstr : "tcp")))
#endif
					{
						eventlog(eventlog_level_error, __FUNCTION__, "could not convert \"{}\" to a port number", portstr);
						return NULL;
					}
#ifdef HAVE_GETSERVBYNAME
					port = ntohs(sp->s_port);
#endif
				}
			}
			else
				port = defport;
		}
		else
			port = defport;

		char addrstr[INET_ADDRSTRLEN] = {};

		if (tstr[0] != '\0')
		{
			hoststr = tstr;
		}
		else
		{
			struct sockaddr_in tsa {};
			tsa.sin_addr.s_addr = htonl(defipaddr);
			
			hoststr = inet_ntop(AF_INET, &(tsa.sin_addr), addrstr, sizeof(addrstr));
		}

		if (!(hostname = host_lookup(hoststr, &ipaddr)))
		{
			eventlog(eventlog_level_error, __FUNCTION__, "could not lookup host \"{}\"", hoststr);
			return NULL;
		}

		temp = new t_addr{};
		{
			char* _tmp = new char[std::strlen(hostname) + 1];
			std::strcpy(_tmp, hostname);
			temp->str = _tmp;
		}

		temp->ip = ipaddr;
		temp->port = port;
		temp->data.p = NULL;

		return temp;
	}


	extern int addr_destroy(t_addr const * addr)
	{
		if (!addr)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got NULL addr");
			return -1;
		}

		if (addr->str)
			delete[] const_cast<char*>(addr->str); /* avoid warning */
		delete const_cast<t_addr*>(addr); /* avoid warning */

		return 0;
	}


	/* hostname or IP */
	extern char * addr_get_host_str(t_addr const * addr, char * str, unsigned int len)
	{
		if (!addr)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got NULL addr");
			return NULL;
		}
		if (!str)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got NULL str");
			return NULL;
		}
		if (len < 2)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "str too short");
			return NULL;
		}

		if (!addr->str)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "addr has NULL str");
			return NULL;
		}

		std::strncpy(str, addr->str, len - 1);
		str[len - 1] = '\0';

		return str;
	}


	/* IP:port */
	extern char * addr_get_addr_str(t_addr const * addr, char * str, unsigned int len)
	{
		if (!addr)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got NULL addr");
			return NULL;
		}
		if (!str)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got NULL str");
			return NULL;
		}
		if (len < 2)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "str too short");
			return NULL;
		}

		std::strncpy(str, addr_num_to_addr_str(addr->ip, addr->port), len - 1);
		str[len - 1] = '\0';

		return str;
	}


	extern unsigned int addr_get_ip(t_addr const * addr)
	{
		if (!addr)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got NULL addr");
			return 0;
		}

		return addr->ip;
	}


	extern unsigned short addr_get_port(t_addr const * addr)
	{
		if (!addr)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got NULL addr");
			return 0;
		}

		return addr->port;
	}


	extern int addr_set_data(t_addr * addr, t_addr_data data)
	{
		if (!addr)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got NULL addr");
			return -1;
		}

		addr->data = data;
		return 0;
	}


	extern t_addr_data addr_get_data(t_addr const * addr)
	{
		t_addr_data tdata;

		if (!addr)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got NULL addr");
			tdata.p = NULL;
			return tdata;
		}

		return addr->data;
	}

#undef HACK_SIZE

} // namespace pvpgn
