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
// R205-split: field-setter tag handlers sub-TU.
// Extracted from handle_apireg_link.cpp (R205) by the R205-split refactor.
//
// Responsibilities (Single Responsibility Principle):
//   All 19 _handle_*_apiregtag functions that simply store a field value
//   into the t_apiregmember struct.  No business logic lives here.

#include "apireg_internal.h"

#include "common/eventlog.h"

namespace pvpgn
{

    namespace bnetd
    {

        int _handle_email_apiregtag(t_apiregmember* apiregmember, char* param)
        {
            if (!apiregmember) {
                ERROR0("got NULL apiregmember");
                return -1;
            }

            if (apiregmember->email)
                delete[] const_cast<char*>(apiregmember->email);

            if (param)
                apiregmember->email = ar_strdup(param);

            return 0;
        }

        int _handle_bmonth_apiregtag(t_apiregmember* apiregmember, char* param)
        {
            if (!apiregmember) {
                ERROR0("got NULL apiregmember");
                return -1;
            }

            if (apiregmember->bmonth)
                delete[] const_cast<char*>(apiregmember->bmonth);

            if (param)
                apiregmember->bmonth = ar_strdup(param);

            return 0;
        }

        int _handle_bday_apiregtag(t_apiregmember* apiregmember, char* param)
        {
            if (!apiregmember) {
                ERROR0("got NULL apiregmember");
                return -1;
            }

            if (apiregmember->bday)
                delete[] const_cast<char*>(apiregmember->bday);

            if (param)
                apiregmember->bday = ar_strdup(param);

            return 0;
        }

        int _handle_byear_apiregtag(t_apiregmember* apiregmember, char* param)
        {
            if (!apiregmember) {
                ERROR0("got NULL apiregmember");
                return -1;
            }

            if (apiregmember->byear)
                delete[] const_cast<char*>(apiregmember->byear);

            if (param)
                apiregmember->byear = ar_strdup(param);

            return 0;
        }

        int _handle_langcode_apiregtag(t_apiregmember* apiregmember, char* param)
        {
            if (!apiregmember) {
                ERROR0("got NULL apiregmember");
                return -1;
            }

            if (apiregmember->langcode)
                delete[] const_cast<char*>(apiregmember->langcode);

            if (param)
                apiregmember->langcode = ar_strdup(param);

            return 0;
        }

        int _handle_sku_apiregtag(t_apiregmember* apiregmember, char* param)
        {
            if (!apiregmember) {
                ERROR0("got NULL apiregmember");
                return -1;
            }

            /* We have SKUs */

            return 0;
        }

        int _handle_ver_apiregtag(t_apiregmember* apiregmember, char* param)
        {
            if (!apiregmember) {
                ERROR0("got NULL apiregmember");
                return -1;
            }

            /* We have VERs */

            return 0;
        }

        int _handle_serial_apiregtag(t_apiregmember* apiregmember, char* param)
        {
            if (!apiregmember) {
                ERROR0("got NULL apiregmember");
                return -1;
            }

            /* We have SERIALs */

            return 0;
        }

        int _handle_sysid_apiregtag(t_apiregmember* apiregmember, char* param)
        {
            if (!apiregmember) {
                ERROR0("got NULL apiregmember");
                return -1;
            }

            if (apiregmember->sysid)
                delete[] const_cast<char*>(apiregmember->sysid);

            if (param)
                apiregmember->sysid = ar_strdup(param);

            return 0;
        }

        int _handle_syscheck_apiregtag(t_apiregmember* apiregmember, char* param)
        {
            if (!apiregmember) {
                ERROR0("got NULL apiregmember");
                return -1;
            }

            if (apiregmember->syscheck)
                delete[] const_cast<char*>(apiregmember->syscheck);

            if (param)
                apiregmember->syscheck = ar_strdup(param);

            return 0;
        }

        int _handle_oldnick_apiregtag(t_apiregmember* apiregmember, char* param)
        {
            if (!apiregmember) {
                ERROR0("got NULL apiregmember");
                return -1;
            }

            /* We have OLDNICKs */

            return 0;
        }

        int _handle_oldpass_apiregtag(t_apiregmember* apiregmember, char* param)
        {
            if (!apiregmember) {
                ERROR0("got NULL apiregmember");
                return -1;
            }

            /* We have OLDPASSs */

            return 0;
        }

        int _handle_newnick_apiregtag(t_apiregmember* apiregmember, char* param)
        {
            if (!apiregmember) {
                ERROR0("got NULL apiregmember");
                return -1;
            }

            if (apiregmember->newnick)
                delete[] const_cast<char*>(apiregmember->newnick);

            if (param)
                apiregmember->newnick = ar_strdup(param);

            return 0;
        }

        int _handle_newpass_apiregtag(t_apiregmember* apiregmember, char* param)
        {
            if (!apiregmember) {
                ERROR0("got NULL apiregmember");
                return -1;
            }

            if (apiregmember->newpass)
                delete[] const_cast<char*>(apiregmember->newpass);

            if (param)
                apiregmember->newpass = ar_strdup(param);

            return 0;
        }

        int _handle_newpass2_apiregtag(t_apiregmember* apiregmember, char* param)
        {
            if (!apiregmember) {
                ERROR0("got NULL apiregmember");
                return -1;
            }

            if (apiregmember->newpass2)
                delete[] const_cast<char*>(apiregmember->newpass2);

            if (param)
                apiregmember->newpass2 = ar_strdup(param);

            return 0;
        }

        int _handle_parentemail_apiregtag(t_apiregmember* apiregmember, char* param)
        {
            if (!apiregmember) {
                ERROR0("got NULL apiregmember");
                return -1;
            }

            if (apiregmember->parentemail)
                delete[] const_cast<char*>(apiregmember->parentemail);

            if (param)
                apiregmember->parentemail = ar_strdup(param);

            return 0;
        }

        int _handle_newsletter_apiregtag(t_apiregmember* apiregmember, char* param)
        {
            if (!apiregmember) {
                ERROR0("got NULL apiregmember");
                return -1;
            }

            /*  if (param) {
                   if (std::strcmp(param, "1") == 0)
                   apiregmember->newsletter = true;
                   else
                   apiregmember->newsletter = false;
                   }*/

            return 0;
        }

        int _handle_shareinfo_apiregtag(t_apiregmember* apiregmember, char* param)
        {
            if (!apiregmember) {
                ERROR0("got NULL apiregmember");
                return -1;
            }

            /*  if (param) {
                   if (std::strcmp(param, "1") == 0)
                   apiregmember->shareinfo = true;
                   else
                   apiregmember->shareinfo = false;
                   }*/

            return 0;
        }

        int _handle_request_apiregtag(t_apiregmember* apiregmember, char* param)
        {
            if (!apiregmember) {
                ERROR0("got NULL apiregmember");
                return -1;
            }

            if (apiregmember->request)
                delete[] const_cast<char*>(apiregmember->request);

            if (param)
                apiregmember->request = ar_strdup(param);

            return 0;
        }

    } // namespace bnetd

} // namespace pvpgn
