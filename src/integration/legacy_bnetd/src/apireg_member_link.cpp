/*
 * Copyright (C) 2007  Pelish (pelish@gmail.com)
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
//
// R205-split: member lifecycle sub-TU.
// Extracted from handle_apireg_link.cpp (R205) by the R205-split refactor.
//
// Responsibilities (Single Responsibility Principle):
//   - ar_strdup: string duplication helper
//   - apiregmember_create / apiregmember_destroy: member object lifecycle
//   - apiregmember_get_* accessors: read-only field access
//   - apireglist_create / apireglist_destroy / apireglist: list management
//   - apireglist_find_apiregmember_by_conn: list lookup

#include "apireg_internal.h"

#include <cstring>

#include "common/eventlog.h"
#include "common/list.h"

namespace pvpgn
{

    namespace bnetd
    {

        /* Module-private list head — owned by this TU. */
        static t_list* apireglist_head = nullptr;

        /* xstrdup-equivalent using new char[] for paired delete[] cleanup. */
        char* ar_strdup(char const* s)
        {
            if (!s) return nullptr;
            std::size_t n = std::strlen(s) + 1;
            char* r = new char[n];
            std::memcpy(r, s, n);
            return r;
        }

        t_apiregmember* apiregmember_create(t_connection* conn)
        {
            t_apiregmember* temp;

            if (!conn) {
                ERROR0("got NULL conn");
                return nullptr;
            }

            temp = new t_apiregmember{};

            eventlog(eventlog_level_info, __FUNCTION__, "creating apiregmember");

            temp->conn        = conn;
            temp->email       = nullptr;
            temp->bday        = nullptr;
            temp->bmonth      = nullptr;
            temp->byear       = nullptr;
            temp->langcode    = nullptr;
            temp->sku         = nullptr;
            temp->ver         = nullptr;
            temp->serial      = nullptr;
            temp->sysid       = nullptr;
            temp->syscheck    = nullptr;
            temp->oldnick     = nullptr;
            temp->oldpass     = nullptr;
            temp->newnick     = nullptr;
            temp->newpass     = nullptr;
            temp->newpass2    = nullptr;
            temp->parentemail = nullptr;
            temp->newsletter  = false;
            temp->shareinfo   = false;
            temp->request     = nullptr;

            list_append_data(apireglist_head, temp);

            return temp;
        }

        int apiregmember_destroy(t_apiregmember* apiregmember, t_elem** curr)
        {
            eventlog(eventlog_level_info, __FUNCTION__, "destroying apiregmember");

            if (!apiregmember) {
                ERROR0("got NULL apiregmember");
                return -1;
            }

            if (list_remove_data(apireglist_head, apiregmember, curr) < 0) {
                ERROR0("could not remove item from list");
                return -1;
            }

            if (apiregmember->email)
                delete[] const_cast<char*>(apiregmember->email);

            if (apiregmember->bday)
                delete[] const_cast<char*>(apiregmember->bday);

            if (apiregmember->bmonth)
                delete[] const_cast<char*>(apiregmember->bmonth);

            if (apiregmember->byear)
                delete[] const_cast<char*>(apiregmember->byear);

            if (apiregmember->langcode)
                delete[] const_cast<char*>(apiregmember->langcode);

            if (apiregmember->sku)
                delete[] const_cast<char*>(apiregmember->sku);

            if (apiregmember->ver)
                delete[] const_cast<char*>(apiregmember->ver);

            if (apiregmember->serial)
                delete[] const_cast<char*>(apiregmember->serial);

            if (apiregmember->sysid)
                delete[] const_cast<char*>(apiregmember->sysid);

            if (apiregmember->syscheck)
                delete[] const_cast<char*>(apiregmember->syscheck);

            if (apiregmember->oldnick)
                delete[] const_cast<char*>(apiregmember->oldnick);

            if (apiregmember->oldpass)
                delete[] const_cast<char*>(apiregmember->oldpass);

            if (apiregmember->newnick)
                delete[] const_cast<char*>(apiregmember->newnick);

            if (apiregmember->newpass)
                delete[] const_cast<char*>(apiregmember->newpass);

            if (apiregmember->newpass2)
                delete[] const_cast<char*>(apiregmember->newpass2);

            if (apiregmember->parentemail)
                delete[] const_cast<char*>(apiregmember->parentemail);

            if (apiregmember->request)
                delete[] const_cast<char*>(apiregmember->request);

            delete apiregmember;

            return 0;
        }

        t_connection* apiregmember_get_conn(t_apiregmember const* apiregmember)
        {
            if (!apiregmember) {
                ERROR0("got NULL apiregmember");
                return nullptr;
            }
            return apiregmember->conn;
        }

        char const* apiregmember_get_email(t_apiregmember const* apiregmember)
        {
            if (!apiregmember) {
                ERROR0("got NULL apiregmember");
                return nullptr;
            }
            return apiregmember->email;
        }

        char const* apiregmember_get_bday(t_apiregmember const* apiregmember)
        {
            if (!apiregmember) {
                ERROR0("got NULL apiregmember");
                return nullptr;
            }
            return apiregmember->bday;
        }

        char const* apiregmember_get_bmonth(t_apiregmember const* apiregmember)
        {
            if (!apiregmember) {
                ERROR0("got NULL apiregmember");
                return nullptr;
            }
            return apiregmember->bmonth;
        }

        char const* apiregmember_get_newnick(t_apiregmember const* apiregmember)
        {
            if (!apiregmember) {
                ERROR0("got NULL apiregmember");
                return nullptr;
            }
            return apiregmember->newnick;
        }

        char const* apiregmember_get_newpass(t_apiregmember const* apiregmember)
        {
            if (!apiregmember) {
                ERROR0("got NULL apiregmember");
                return nullptr;
            }
            return apiregmember->newpass;
        }

        char const* apiregmember_get_request(t_apiregmember const* apiregmember)
        {
            if (!apiregmember) {
                ERROR0("got NULL apiregmember");
                return nullptr;
            }
            return apiregmember->request;
        }

        // ----------------------------------------------------------------
        // List management
        // ----------------------------------------------------------------

        extern int apireglist_create(void)
        {
            apireglist_head = list_create();
            return 0;
        }

        extern int apireglist_destroy(void)
        {
            t_apiregmember* apiregmember;
            t_elem*         curr;

            if (apireglist_head) {
                LIST_TRAVERSE(apireglist_head, curr) {
                    if (!(apiregmember = (t_apiregmember*)elem_get_data(curr))) {
                        ERROR0("channel list contains NULL item");
                        continue;
                    }
                    apiregmember_destroy(apiregmember, &curr);
                }

                if (list_destroy(apireglist_head) < 0)
                    return -1;
                apireglist_head = nullptr;
            }

            return 0;
        }

        extern t_list* apireglist(void)
        {
            return apireglist_head;
        }

        t_apiregmember* apireglist_find_apiregmember_by_conn(t_connection* conn)
        {
            t_elem* curr;

            if (!conn) {
                ERROR0("got NULL conn");
                return nullptr;
            }

            LIST_TRAVERSE(apireglist(), curr) {
                t_apiregmember* apiregmember = (t_apiregmember*)elem_get_data(curr);
                if (conn == apiregmember_get_conn(apiregmember)) {
                    return apiregmember;
                }
            }

            return nullptr;
        }

    } // namespace bnetd

} // namespace pvpgn
