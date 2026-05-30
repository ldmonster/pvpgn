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
// R205-split: private internal header shared across the apireg sub-TUs.
// Not part of the public include tree — only visible to files in this
// src/ directory that explicitly include it.
//
// Sub-TU layout:
//   apireg_member_link.cpp  — ar_strdup, apiregmember_create/destroy,
//                             all apiregmember_get_* accessors,
//                             apireglist_create/destroy/find/apireglist
//   apireg_tags_link.cpp    — all 19 _handle_*_apiregtag field-setter
//                             functions + apireg_tag_table dispatch table
//   apireg_request_link.cpp — _handle_end_apiregtag (AGEVERIFY + GETNICK
//                             account-creation business logic)
//   handle_apireg_link.cpp  — thin coordinator: handle_apireg_tag,
//                             handle_apireg_line, handle_apireg_packet,
//                             apireg_send

#pragma once

#define APIREGISTER_INTERNAL_ACCESS
#include "common/setup_before.h"
#include "handle_apireg.h"
#include "common/setup_after.h"

namespace pvpgn
{
    namespace bnetd
    {

        // ----------------------------------------------------------------
        // Shared string helper (defined in apireg_member_link.cpp)
        // ----------------------------------------------------------------
        char* ar_strdup(char const* s);

        // ----------------------------------------------------------------
        // Member lifecycle (defined in apireg_member_link.cpp)
        // ----------------------------------------------------------------
        t_apiregmember* apiregmember_create(t_connection* conn);
        int             apiregmember_destroy(t_apiregmember* apiregmember, t_elem** curr);

        // ----------------------------------------------------------------
        // Member accessors (defined in apireg_member_link.cpp)
        // ----------------------------------------------------------------
        t_connection*  apiregmember_get_conn(t_apiregmember const* apiregmember);
        char const*    apiregmember_get_email(t_apiregmember const* apiregmember);
        char const*    apiregmember_get_bday(t_apiregmember const* apiregmember);
        char const*    apiregmember_get_bmonth(t_apiregmember const* apiregmember);
        char const*    apiregmember_get_newnick(t_apiregmember const* apiregmember);
        char const*    apiregmember_get_newpass(t_apiregmember const* apiregmember);
        char const*    apiregmember_get_request(t_apiregmember const* apiregmember);

        // ----------------------------------------------------------------
        // List management (defined in apireg_member_link.cpp)
        // ----------------------------------------------------------------
        t_apiregmember* apireglist_find_apiregmember_by_conn(t_connection* conn);

        // ----------------------------------------------------------------
        // Field-setter tag handlers (defined in apireg_tags_link.cpp)
        // ----------------------------------------------------------------
        int _handle_email_apiregtag(t_apiregmember* apiregmember, char* param);
        int _handle_bmonth_apiregtag(t_apiregmember* apiregmember, char* param);
        int _handle_bday_apiregtag(t_apiregmember* apiregmember, char* param);
        int _handle_byear_apiregtag(t_apiregmember* apiregmember, char* param);
        int _handle_langcode_apiregtag(t_apiregmember* apiregmember, char* param);
        int _handle_sku_apiregtag(t_apiregmember* apiregmember, char* param);
        int _handle_ver_apiregtag(t_apiregmember* apiregmember, char* param);
        int _handle_serial_apiregtag(t_apiregmember* apiregmember, char* param);
        int _handle_sysid_apiregtag(t_apiregmember* apiregmember, char* param);
        int _handle_syscheck_apiregtag(t_apiregmember* apiregmember, char* param);
        int _handle_oldnick_apiregtag(t_apiregmember* apiregmember, char* param);
        int _handle_oldpass_apiregtag(t_apiregmember* apiregmember, char* param);
        int _handle_newnick_apiregtag(t_apiregmember* apiregmember, char* param);
        int _handle_newpass_apiregtag(t_apiregmember* apiregmember, char* param);
        int _handle_newpass2_apiregtag(t_apiregmember* apiregmember, char* param);
        int _handle_parentemail_apiregtag(t_apiregmember* apiregmember, char* param);
        int _handle_newsletter_apiregtag(t_apiregmember* apiregmember, char* param);
        int _handle_shareinfo_apiregtag(t_apiregmember* apiregmember, char* param);
        int _handle_request_apiregtag(t_apiregmember* apiregmember, char* param);

        // ----------------------------------------------------------------
        // Business-logic END handler (defined in apireg_request_link.cpp)
        // ----------------------------------------------------------------
        int _handle_end_apiregtag(t_apiregmember* apiregmember, char* param);

        // ----------------------------------------------------------------
        // Send helper (defined in handle_apireg_link.cpp)
        // ----------------------------------------------------------------
        int apireg_send(t_connection* conn, char const* command);

    } // namespace bnetd
} // namespace pvpgn
