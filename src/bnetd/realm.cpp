/*
 * Copyright (C) 2000  Ross Combs (rocombs@cs.nmsu.edu)
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
#include "common/setup_before.h"
#define REALM_INTERNAL_ACCESS
#include "realm.h"

#include <cstdio>
#include <cerrno>
#include <cstring>
#include <cassert>
#include <vector>
#include <algorithm>

#include "common/eventlog.h"
#include "common/addr.h"
#include "common/util.h"

#include <strings.h>

#include "connection.h"
#include "common/setup_after.h"


namespace pvpgn
{

	namespace bnetd
	{


		/* xstrdup-equivalent using new char[] for paired delete[] cleanup. */
		static char* rl_strdup(char const* s)
		{
			if (!s) return nullptr;
			std::size_t n = std::strlen(s) + 1;
			char* r = new char[n];
			std::memcpy(r, s, n);
			return r;
		}
		static std::vector<t_realm*> realmlist_head;

		static t_realm * realm_create(char const * name, char const * description, unsigned int ip, unsigned int port);
		static int realm_destroy(t_realm * realm);

		static t_realm * realm_create(char const * name, char const * description, unsigned int ip, unsigned int port)
		{
			t_realm * realm;

			if (!name)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL name");
				return NULL;
			}
			if (!description)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL description");
				return NULL;
			}

			realm = new t_realm{};
			realm->name = NULL;
			realm->description = NULL;

			if (realm_set_name(realm, name) < 0)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "failed to set name for realm");
				delete realm;
				return NULL;
			}
			if (realm->description != NULL) delete[] const_cast<char*>(realm->description);
			realm->description = rl_strdup(description);
			realm->ip = ip;
			realm->port = port;
			realm->conn = NULL;
			realm->active = 0;
			realm->player_number = 0;
			realm->game_number = 0;
			realm->sessionnum = 0;
			realm->tcp_sock = 0;
			rcm_init(&realm->rcm);

			eventlog(eventlog_level_info, __FUNCTION__, "created realm \"{}\"", name);
			return realm;
		}


		static int realm_destroy(t_realm * realm)
		{
			if (!realm)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL realm");
				return -1;
			}

			if (realm->active)
				realm_deactive(realm);

			delete[] const_cast<char*>(realm->name); /* avoid warning */
			delete[] const_cast<char*>(realm->description); /* avoid warning */
			delete realm; /* avoid warning */

			return 0;
		}


		extern char const * realm_get_name(t_realm const * realm)
		{
			if (!realm)
			{
				return NULL;
			}
			return realm->name;
		}


		extern int realm_set_name(t_realm * realm, char const * name)
		{
			char const      * temp;
			t_realm const * temprealm;

			if (!realm)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL realm");
				return -1;
			}

			if (name && (temprealm = realmlist_find_realm(name)))
			{
				if (temprealm == realm)
					return 0;
				else
				{
					eventlog(eventlog_level_error, __FUNCTION__, "realm {} does already exist in list", name);
					return -1;
				}
			}

			if (name)
				temp = rl_strdup(name);
			else
				temp = NULL;

			if (realm->name)
				delete[] const_cast<char*>(realm->name); /* avoid warning */
			realm->name = temp;

			return 0;
		}


		extern char const * realm_get_description(t_realm const * realm)
		{
			if (!realm)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL realm");
				return NULL;
			}
			return realm->description;
		}


		extern unsigned short realm_get_port(t_realm const * realm)
		{
			if (!realm)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL realm");
				return 0;
			}
			return realm->port;
		}


		extern unsigned int realm_get_ip(t_realm const * realm)
		{
			if (!realm)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL realm");
				return 0;
			}
			return realm->ip;
		}


		extern unsigned int realm_get_active(t_realm const * realm)
		{
			if (!realm)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL realm");
				return 0;
			}
			return realm->active;
		}

		extern int realm_set_active(t_realm * realm, unsigned int active)
		{
			if (!realm)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL realm");
				return -1;
			}

			realm->active = active;
			return 0;
		}

		extern unsigned int realm_get_player_number(t_realm const * realm)
		{
			if (!realm)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL realm");
				return 0;
			}
			return realm->player_number;
		}

		extern int realm_add_player_number(t_realm * realm, int number)
		{
			if (!realm)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL realm");
				return -1;
			}
			realm->player_number += number;
			return 0;
		}

		extern unsigned int realm_get_game_number(t_realm const * realm)
		{
			if (!realm)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL realm");
				return 0;
			}
			return realm->game_number;
		}

		extern int realm_add_game_number(t_realm * realm, int number)
		{
			if (!realm)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL realm");
				return -1;
			}
			realm->game_number += number;
			return 0;
		}

		extern int realm_active(t_realm * realm, t_connection * c)
		{
			if (!realm)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL realm");
				return -1;
			}
			if (!c)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL connection");
				return -1;
			}
			if (realm->active)
			{
				eventlog(eventlog_level_debug, __FUNCTION__, "realm {} is already actived,destroy previous one", realm->name);
				realm_deactive(realm);
			}
			realm->active = 1;
			realm->conn = c;
			conn_set_realm(c, realm);
			realm->sessionnum = conn_get_sessionnum(c);
			realm->tcp_sock = conn_get_socket(c);
			eventlog(eventlog_level_info, __FUNCTION__, "realm {} actived", realm->name);
			return 0;
		}

		extern int realm_deactive(t_realm * realm)
		{
			t_connection * c;

			if (!realm)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL realm");
				return -1;
			}
			if (!realm->active)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "realm {} is not actived", realm->name);
				return -1;
			}
			if ((c = realm_get_conn(realm)))
				conn_set_state(c, conn_state_destroy);

			realm->active = 0;
			realm->sessionnum = 0;
			realm->tcp_sock = 0;
			/*
			realm->player_number=0;
			realm->game_number=0;
			*/
			eventlog(eventlog_level_info, __FUNCTION__, "realm {} deactived", realm->name);
			return 0;
		}


		static std::vector<t_realm*> realmlist_load(char const * filename)
		{
			std::FILE *          fp;
			unsigned int    line;
			unsigned int    pos;
			unsigned int    len;
			t_addr *        raddr;
			char *          temp, *temp2;
			char *          buff;
			char *          name;
			char *          desc;
			t_realm *       realm;
			std::vector<t_realm*> result;

			if (!filename)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL filename");
				return result;
			}

			if (!(fp = std::fopen(filename, "r")))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "could not open realm file \"{}\" for reading (std::fopen: {})", filename, std::strerror(errno));
				return result;
			}

			for (line = 1; (buff = file_get_line(fp)); line++)
			{
				for (pos = 0; buff[pos] == '\t' || buff[pos] == ' '; pos++);
				if (buff[pos] == '\0' || buff[pos] == '#')
				{
					continue;
				}
				if ((temp = std::strrchr(buff, '#')))
				{
					unsigned int endpos;

					*temp = '\0';
					len = std::strlen(buff) + 1;
					for (endpos = len - 1; buff[endpos] == '\t' || buff[endpos] == ' '; endpos--);
					buff[endpos + 1] = '\0';
				}

				/* skip any separators */
				for (temp = buff; *temp && (*temp == ' ' || *temp == '\t'); temp++);
				if (*temp != '"') {
					eventlog(eventlog_level_error, __FUNCTION__, "malformed line {} in file \"{}\" (no realmname)", line, filename);
					continue;
				}

				temp2 = temp + 1;
				/* find the next " */
				for (temp = temp2; *temp && *temp != '"'; temp++);
				if (*temp != '"' || temp == temp2) {
					eventlog(eventlog_level_error, __FUNCTION__, "malformed line {} in file \"{}\" (no realmname)", line, filename);
					continue;
				}

				/* save the realmname */
				*temp = '\0';
				name = rl_strdup(temp2);

				/* eventlog(eventlog_level_trace, __FUNCTION__,"found realmname: {}",name); */

				/* skip any separators */
				for (temp = temp + 1; *temp && (*temp == '\t' || *temp == ' '); temp++);

				if (*temp == '"') { /* we have realm description */
					temp2 = temp + 1;
					/* find the next " */
					for (temp = temp2; *temp && *temp != '"'; temp++);
					if (*temp != '"' || temp == temp2) {
						eventlog(eventlog_level_error, __FUNCTION__, "malformed line {} in file \"{}\" (no valid description)", line, filename);
						delete[] name;
						continue;
					}

					/* save the description */
					*temp = '\0';
					desc = rl_strdup(temp2);

					/* eventlog(eventlog_level_trace, __FUNCTION__,"found realm desc: {}",desc); */

					/* skip any separators */
					for (temp = temp + 1; *temp && (*temp == ' ' || *temp == '\t'); temp++);
				}
				else desc = rl_strdup("\0");

				temp2 = temp;
				/* find out where address ends */
				for (temp = temp2 + 1; *temp && *temp != ' ' && *temp != '\t'; temp++);

				if (*temp) *temp++ = '\0'; /* if is not the end of the file, end addr and move forward */

				/* eventlog(eventlog_level_trace, __FUNCTION__,"found realm ip: {}",temp2); */

				if (!(raddr = addr_create_str(temp2, 0, BNETD_REALM_PORT))) /* 0 means "this computer" */ {
					eventlog(eventlog_level_error, __FUNCTION__, "invalid address value for field 3 on line {} in file \"{}\"", line, filename);
					delete[] name;
					delete[] desc;
					continue;
				}

				if (!(realm = realm_create(name, desc, addr_get_ip(raddr), addr_get_port(raddr))))
				{
					eventlog(eventlog_level_error, __FUNCTION__, "could not create realm");
					addr_destroy(raddr);
					delete[] name;
					delete[] desc;
					continue;
				}

				addr_destroy(raddr);
				delete[] name;
				delete[] desc;

				result.push_back(realm);
			}
			file_get_line(NULL); // clear file_get_line buffer
			if (std::fclose(fp) < 0)
				eventlog(eventlog_level_error, __FUNCTION__, "could not close realm file \"{}\" after reading (std::fclose: {})", filename, std::strerror(errno));
			return result;
		}

		extern int realmlist_reload(char const * filename)
		{
			std::vector<t_realm*> newlist;
			std::vector<t_realm*> oldlist;

			oldlist = std::move(realmlist_head);
			realmlist_head.clear();

			newlist = realmlist_load(filename);
			if (newlist.empty())
				return -1;

			for (t_realm* old_realm : oldlist)
			{
				bool match = false;
				for (t_realm* new_realm : newlist)
				{
					if (!std::strcmp(old_realm->name, new_realm->name))
					{
						match = true;
						rcm_chref(&old_realm->rcm, new_realm);
						break;
					}
				}
				if (!match)
					rcm_chref(&old_realm->rcm, NULL);

				realm_destroy(old_realm);
			}

			realmlist_head = std::move(newlist);

			return 0;
		}

		extern int realmlist_create(char const * filename)
		{
			realmlist_head = realmlist_load(filename);
			if (realmlist_head.empty())
				return -1;

			return 0;
		}

		static void realmlist_unload(std::vector<t_realm*>& list)
		{
			for (t_realm* realm : list)
				realm_destroy(realm);
			list.clear();
		}

		extern int realmlist_destroy()
		{
			realmlist_unload(realmlist_head);
			return 0;
		}

		extern const std::vector<t_realm*>& realmlist(void)
		{
			return realmlist_head;
		}


		extern t_realm * realmlist_find_realm(char const * realmname)
		{
			if (!realmname)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "got NULL realmname");
				return NULL;
			}

			for (t_realm* realm : realmlist_head)
			{
				if (strcasecmp(realm->name, realmname) == 0)
					return realm;
			}

			return NULL;
		}

		extern t_realm * realmlist_find_realm_by_ip(unsigned long ip)
		{
			for (t_realm* realm : realmlist_head)
			{
				if (realm->ip == ip)
					return realm;
			}
			return NULL;
		}

		extern t_connection * realm_get_conn(t_realm * realm)
		{
			assert(realm);

			return realm->conn;
		}

		extern t_realm * realm_get(t_realm * realm, t_rcm_regref * regref)
		{
			rcm_get(&realm->rcm, regref);
			return realm;
		}

		extern void realm_put(t_realm * realm, t_rcm_regref * regref)
		{
			rcm_put(&realm->rcm, regref);
		}

	}

}
