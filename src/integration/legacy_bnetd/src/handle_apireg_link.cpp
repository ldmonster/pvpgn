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
// R205: relocated from `src/bnetd/handle_apireg.cpp`.
// Strangler-fig move into the v3 `integration_legacy_bnetd_linked` library.
// Symbol surface preserved verbatim. The legacy
// `#ifdef PVPGN_V3_BNETD_INTEGRATION` guards are dropped because we are
// unconditionally inside the v3 build here.
//
// R205-split: this file is now a thin coordinator.
// The 935-LOC monolith has been split by concern into focused sub-TUs:
//
//   apireg_member_link.cpp  — ar_strdup, apiregmember_create/destroy,
//                             all apiregmember_get_* accessors,
//                             apireglist_create/destroy/find/apireglist
//   apireg_tags_link.cpp    — all 19 _handle_*_apiregtag field-setter
//                             functions (no business logic)
//   apireg_request_link.cpp — _handle_end_apiregtag (AGEVERIFY + GETNICK
//                             account-creation business logic)
//   handle_apireg_link.cpp  — THIS FILE: thin coordinator
//                             handle_apireg_tag (dispatch table lookup)
//                             handle_apireg_line (line parser)
//                             handle_apireg_packet (packet framer)
//                             apireg_send (send + cleanup)

#include "apireg_internal.h"

#include <cstring>
#include <cstdio>

#include "common/eventlog.h"
#include "common/list.h"
#include "common/packet.h"

#include "irc.h"

// Send-bridge: encodes a raw-text packet and dispatches via send_packet handler.
// Returns 1 (handled), 0 (fall through), -1 (error).
extern "C" int pvpgn_v3_send_raw_text(void* conn_ptr, char const* text) noexcept;

namespace pvpgn
{

    namespace bnetd
    {

        typedef int(*t_apireg_tag)(t_apiregmember* apiregmember, char* param);

        typedef struct {
            const char*  apireg_tag_string;
            t_apireg_tag apireg_tag_handler;
        } t_apireg_tag_table_row;

        static const t_apireg_tag_table_row apireg_tag_table[] =
        {
            { "EMAIL",       _handle_email_apiregtag       },
            { "BMONTH",      _handle_bmonth_apiregtag      },
            { "BDAY",        _handle_bday_apiregtag        },
            { "BYEAR",       _handle_byear_apiregtag       },
            { "LANGCODE",    _handle_langcode_apiregtag    },
            { "SKU",         _handle_sku_apiregtag         },
            { "VER",         _handle_ver_apiregtag         },
            { "SERIAL",      _handle_serial_apiregtag      },
            { "SYSID",       _handle_sysid_apiregtag       },
            { "SYSCHECK",    _handle_syscheck_apiregtag    },
            { "OLDNICK",     _handle_oldnick_apiregtag     },
            { "OLDPASS",     _handle_oldpass_apiregtag     },
            { "NEWNICK",     _handle_newnick_apiregtag     },
            { "NEWPASS",     _handle_newpass_apiregtag     },
            { "NEWPASS2",    _handle_newpass2_apiregtag    },
            { "PARENTEMAIL", _handle_parentemail_apiregtag },
            { "NEWSLETTER",  _handle_newsletter_apiregtag  },
            { "SHAREINFO",   _handle_shareinfo_apiregtag   },
            { "REQUEST",     _handle_request_apiregtag     },
            { "END",         _handle_end_apiregtag         },

            { nullptr, nullptr }
        };

        // ----------------------------------------------------------------
        // Tag dispatcher — walks the table and calls the matching handler.
        // ----------------------------------------------------------------
        static int handle_apireg_tag(t_apiregmember* apiregmember, char const* tag, char* param)
        {
            t_apireg_tag_table_row const* p;

            for (p = apireg_tag_table; p->apireg_tag_string != nullptr; p++) {
                if (strcasecmp(tag, p->apireg_tag_string) == 0) {
                    if (p->apireg_tag_handler != nullptr)
                        return ((p->apireg_tag_handler)(apiregmember, param));
                }
            }
            return -1;
        }

        // ----------------------------------------------------------------
        // Line parser — splits "TAG=param" and dispatches to handle_apireg_tag.
        // ----------------------------------------------------------------
        static int handle_apireg_line(t_connection* conn, char const* apiregline)
        {
            /* <command>=[param] */
            char*           line;         /* copy of apiregline */
            char*           tag   = nullptr; /* mandatory */
            char*           param = nullptr; /* param of tag */
            t_apiregmember* apiregmember = apireglist_find_apiregmember_by_conn(conn);

            if (!conn) {
                ERROR0("got NULL connection");
                return -1;
            }
            if (!apiregline) {
                ERROR0("got NULL apiregline");
                return -1;
            }
            if (apiregline[0] == '\0') {
                ERROR0("got empty apiregline");
                return -1;
            }

            if (std::strlen(apiregline) > 254) {
                char* tmp = (char*)apiregline;
                WARN0("line to long, truncation...");
                tmp[254] = '\0';
            }

            if (!apiregmember) {
                apiregmember = apiregmember_create(conn);
            }
            line = ar_strdup(apiregline);

            /* split the line */
            tag   = line;
            param = std::strchr(tag, '=');
            if (param)
                *param++ = '\0';

            eventlog(eventlog_level_debug, __FUNCTION__,
                "[{}] got \"{}\" [{}]",
                conn_get_socket(conn), tag, ((param) ? (param) : ("")));

            if (handle_apireg_tag(apiregmember, tag, param) != -1) {}
            delete[] line;

            return 0;
        }

        // ----------------------------------------------------------------
        // Packet framer — reassembles newline-delimited lines from raw bytes.
        // ----------------------------------------------------------------
        extern int handle_apireg_packet(t_connection* conn, t_packet const* const packet)
        {
            unsigned int i;
            char         apiregline[MAX_IRC_MESSAGE_LEN];
            char const*  data;

            if (!packet) {
                ERROR0("got NULL packet");
                return -1;
            }
            if (conn_get_class(conn) != conn_class_apireg) {
                ERROR0("FIXME: handle_apireg_packet without any reason (conn->class != conn_class_apireg)");
                return -1;
            }

            std::memset(apiregline, 0, sizeof(apiregline));

            data = conn_get_ircline(conn); /* fetch current status */
            if (data) {
                std::snprintf(apiregline, sizeof apiregline, "%s", data);
            }

            unsigned apiregpos = std::strlen(apiregline);
            data = (const char*)packet_get_raw_data_const(packet, 0);

            for (i = 0; i < packet_get_size(packet); i++) {
                if ((data[i] == '\r') || (data[i] == '\0')) {
                    /* kindly ignore \r and NUL ... */
                }
                else if (data[i] == '\n') {
                    /* end of line */
                    handle_apireg_line(conn, apiregline);
                    std::memset(apiregline, 0, sizeof(apiregline));
                    apiregpos = 0;
                }
                else {
                    if (apiregpos < MAX_IRC_MESSAGE_LEN - 1)
                        apiregline[apiregpos++] = data[i];
                    else {
                        apiregpos++; /* for the statistic :) */
                        WARN2("[{}] client exceeded maximum allowed message length by {} characters",
                            conn_get_socket(conn), apiregpos - MAX_IRC_MESSAGE_LEN);
                        if (apiregpos > 100 + MAX_IRC_MESSAGE_LEN) {
                            /* automatic flood protection */
                            ERROR1("[{}] excess flood", conn_get_socket(conn));
                            return -1;
                        }
                    }
                }
            }
            conn_set_ircline(conn, apiregline); /* write back current status */
            return 0;
        }

        // ----------------------------------------------------------------
        // Send helper — serialises response, dispatches via v3 bridge or
        // legacy outqueue, then destroys the member and closes the connection.
        // ----------------------------------------------------------------
        int apireg_send(t_connection* conn, char const* command)
        {
            char     data[MAX_IRC_MESSAGE_LEN + 1];
            unsigned len = 0;
            t_elem*  curr;

            if (command)
                len = (std::strlen(command));

            if (len > MAX_IRC_MESSAGE_LEN) {
                ERROR1("message to send is too large ({} bytes)", len);
                return -1;
            }
            else {
                std::sprintf(data, "%s", command);
            }

            DEBUG2("[{}] sent \"{}\"", conn_get_socket(conn), data);
            {
                int const _rc = pvpgn_v3_send_raw_text(conn, data);
                if (_rc == 1) goto apireg_send_skip_legacy;
                if (_rc == -1) return -1;
            }
            {
                t_packet* const p = packet_create(packet_class_raw);
                packet_set_size(p, 0);
                packet_append_data(p, data, len);
                conn_push_outqueue(conn, p);
                packet_del_ref(p);
            }
            apireg_send_skip_legacy:;

            /* In apiregister server we must destroy apiregmember and connection after send packet */

            LIST_TRAVERSE(apireglist(), curr) {
                t_apiregmember* tempapireg = (t_apiregmember*)elem_get_data(curr);

                if (conn == apiregmember_get_conn(tempapireg))
                    apiregmember_destroy(tempapireg, &curr);
            }

            conn_set_state(conn, conn_state_destroy);

            return 0;
        }

    } // namespace bnetd

} // namespace pvpgn
