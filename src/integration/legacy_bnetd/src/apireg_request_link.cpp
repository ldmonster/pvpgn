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
// R205-split: END-tag business-logic sub-TU.
// Extracted from handle_apireg_link.cpp (R205) by the R205-split refactor.
//
// Responsibilities (Single Responsibility Principle):
//   _handle_end_apiregtag — the sole function with real business logic:
//     - REQUEST_AGEVERIFY: compute/return age + consent fields
//     - REQUEST_GETNICK:   validate nick/pass, create account, set WOL hash
//   All other tag handlers live in apireg_tags_link.cpp.

#include "apireg_internal.h"

#include <cstring>
#include <cstdio>

#include "common/eventlog.h"
#include "common/bnethash.h"
#include "common/wolhash.h"
#include "common/list.h"

#include "prefs_v3_shim.h"
#include "account.h"
#include "account_wrap.h"
#include "irc.h"

namespace pvpgn
{

    namespace bnetd
    {

        int _handle_end_apiregtag(t_apiregmember* apiregmember, char* param)
        {
            char data[MAX_IRC_MESSAGE_LEN];
            char temp[MAX_IRC_MESSAGE_LEN];
            t_connection* conn    = apiregmember_get_conn(apiregmember);
            t_elem*       curr;
            t_account*    account;
            char const*   newnick  = apiregmember_get_newnick(apiregmember);
            char const*   newpass  = apiregmember_get_newpass(apiregmember);
            char const*   email    = apiregmember_get_email(apiregmember);
            char const*   request  = apiregmember_get_request(apiregmember);
            char hresult[12];
            char message[MAX_IRC_MESSAGE_LEN];
            char age[8];
            char consent[12];

            std::memset(data,    0, sizeof(data));
            std::memset(temp,    0, sizeof(temp));
            std::memset(hresult, 0, sizeof(hresult));
            std::memset(message, 0, sizeof(message));
            std::memset(age,     0, sizeof(age));
            std::memset(consent, 0, sizeof(consent));

            std::snprintf(hresult, sizeof(hresult), "0");
            std::snprintf(message, sizeof(message), "((Message))");
            std::snprintf(age,     sizeof(age),     "((Age))");
            std::snprintf(consent, sizeof(consent), "((Consent))");

            DEBUG3("APIREG:/{}/{}/{}/",
                apiregmember_get_request(apiregmember),
                apiregmember_get_newnick(apiregmember),
                apiregmember_get_newpass(apiregmember));

            if ((request) && (std::strcmp(apiregmember_get_request(apiregmember), REQUEST_AGEVERIFY) == 0)) {
                std::snprintf(data, sizeof(data),
                    "HRESULT=%.11s\nMessage=%.256s\nNewNick=((NewNick))\nNewPass=((NewPass))\n",
                    hresult, message);
                /* FIXME: Count real age here! */
                std::snprintf(age,  sizeof(age),  "28"); /* FIXME: Here must be counted age */
                std::snprintf(temp, sizeof(temp), "Age=%s\nConsent=((Consent))\nEND\r", age);
                std::strncat(data, temp, sizeof(data) - std::strlen(data) - 1);
                apireg_send(apiregmember_get_conn(apiregmember), data);
                return 0;
            }
            else if ((request) && (std::strcmp(apiregmember_get_request(apiregmember), REQUEST_GETNICK) == 0)) {
                if (!prefs_v3::allow_new_accounts()) {
                    std::snprintf(message, sizeof(message), "Account creation is not allowed");
                    std::snprintf(hresult, sizeof(hresult), "-2147221248");
                }
                else {
                    if (!newnick) {
                        std::snprintf(message, sizeof(message), "Nick must be specifed!");
                        std::snprintf(hresult, sizeof(hresult), "-2147221248");
                    }
                    else if (!newpass) {
                        std::snprintf(message, sizeof(message), "Pussword must be specifed!");
                        std::snprintf(hresult, sizeof(hresult), "-2147221248");
                    }
                    else if ((account = accountlist_find_account(newnick))) {
                        std::snprintf(message, sizeof(message),
                            "That login is already in use! Please try another NICK name.");
                        std::snprintf(hresult, sizeof(hresult), "-2147221248");
                    }
                    else {
                        /* done, we can create new account */
                        t_account*  tempacct;
                        t_hash      bnet_pass_hash;
                        t_wolhash   wol_pass_hash;

                        /* Here we can also check serials and/or emails... */

                        bnet_hash(&bnet_pass_hash, std::strlen(newpass), newpass);
                        wol_hash(&wol_pass_hash,   std::strlen(newpass), newpass);

                        tempacct = accountlist_create_account(newnick, hash_get_str(bnet_pass_hash));

                        if (!tempacct) {
                            // ERROR: Account is not created! - Why? :)
                            return 0;
                        }
                        else {
                            eventlog(eventlog_level_debug, __FUNCTION__, "WOLHASH: {}", wol_pass_hash);
                            account_set_wol_apgar(tempacct, wol_pass_hash);
                            if (apiregmember_get_email(apiregmember))
                                account_set_email(tempacct, apiregmember_get_email(apiregmember));
                            std::snprintf(message, sizeof(message),
                                "Welcome in the amazing world of PvPGN! "
                                "Your login can be used for all PvPGN Supported games!");
                            std::snprintf(hresult, sizeof(hresult), "0");
                        }
                    }
                }
                std::snprintf(data, sizeof(data),
                    "HRESULT=%.11s\nMessage=%.256s\nNewNick=%.32s\nNewPass=%.32s\n"
                    "Age=%.7s\nConsent=%.11s\nEND\r",
                    hresult, message,
                    newnick ? newnick : "",
                    newpass ? newpass : "",
                    age, consent);
                apireg_send(apiregmember_get_conn(apiregmember), data);
                return 0;
            }
            else {
                /* Error: Unknown request - closing connection */
                ERROR1("got UNKNOWN request /{}/ closing connection", apiregmember->request);
                LIST_TRAVERSE(apireglist(), curr) {
                    t_apiregmember* apiregmemberlist = (t_apiregmember*)elem_get_data(curr);

                    if (conn == apiregmember_get_conn(apiregmemberlist)) {
                        apiregmember_destroy(apiregmember, &curr);
                        break;
                    }
                }

                conn_set_state(conn, conn_state_destroy);
                return 0;
            }

            return 0;
        }

    } // namespace bnetd

} // namespace pvpgn
