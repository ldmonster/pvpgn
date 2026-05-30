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
// Address list (t_addrlist) creation, destruction, append, and length query (plan 15 §3 / SOLID-S).
// Included as a sub-TU by addr.cpp — do not compile directly.

namespace pvpgn
{

	extern int addrlist_append(t_addrlist * addrlist, char const * str, unsigned int defipaddr, unsigned short defport)
	{
		t_addr *     addr;
		char *       tok;

		assert(addrlist != NULL);

		if (!str)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got NULL str");
			return -1;
		}

		std::string tstr_storage = str;
		char* tstr = tstr_storage.empty() ? nullptr : &tstr_storage[0];
		for (tok = std::strtok(tstr, ","); tok; tok = std::strtok(NULL, ",")) /* std::strtok modifies the string it is passed */
		{
			if (!(addr = addr_create_str(tok, defipaddr, defport)))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "could not create addr");
				return -1;
			}
			list_append_data(addrlist, addr);
		}

		return 0;
	}

	extern t_addrlist * addrlist_create(char const * str, unsigned int defipaddr, unsigned short defport)
	{
		t_addrlist * addrlist;

		if (!str)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got NULL str");
			return NULL;
		}

		addrlist = list_create();

		if (addrlist_append(addrlist, str, defipaddr, defport) < 0) {
			eventlog(eventlog_level_error, __FUNCTION__, "could not append to newly created addrlist");
			list_destroy(addrlist);
			return NULL;
		}

		return addrlist;
	}

	extern int addrlist_destroy(t_addrlist * addrlist)
	{
		t_elem * curr;
		t_addr * addr;

		if (!addrlist)
		{
			eventlog(eventlog_level_error, __FUNCTION__, "got NULL addrlist");
			return -1;
		}

		LIST_TRAVERSE(addrlist, curr)
		{
			if (!(addr = (t_addr*)elem_get_data(curr)))
				eventlog(eventlog_level_error, __FUNCTION__, "found NULL addr in list");
			else
				addr_destroy(addr);
			list_remove_elem(addrlist, &curr);
		}

		return list_destroy(addrlist);
	}


	extern int addrlist_get_length(t_addrlist const * addrlist)
	{
		return list_get_length(addrlist);
	}

} // namespace pvpgn
