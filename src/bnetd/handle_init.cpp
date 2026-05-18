/*
 * Copyright (C) 1999,2000  Ross Combs (rocombs@cs.nmsu.edu)
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
#include "handle_init.h"

#include "common/init_protocol.h"
#include "common/eventlog.h"
#include "common/packet.h"
#include "common/bn_type.h"
#include "common/addr.h"

#include "prefs.h"
#include "connection.h"
#include "realm.h"
#include "handle_d2cs.h"
#include "common/setup_after.h"

#ifdef PVPGN_V3_BNETD_INTEGRATION
// Strangler-fig observer (Step 4 E.3): forward-declared inline so
// we don't have to pull a C++ header into this TU. The bridge is
// defined in `integration_legacy_bnetd` and resolves the same
// byte-to-decision table that the legacy switch below encodes; we
// compare and log on any divergence. v3 does NOT yet apply the
// decision -- the legacy code remains authoritative until the
// parity log goes silent across a full bnetd run.
extern "C" int pvpgn_v3_init_conn_decide(unsigned char cclass,
                                         unsigned char* out_decision) noexcept;

// Authoritative apply hook (Step 4 E.3 follow-up): when a handler
// has been installed via `install_init_conn_apply_handler()`, this
// returns 1 (v3 transitioned the conn), -1 (handled but failed --
// caller should return -1), or 0 (declined; legacy switch runs).
extern "C" int pvpgn_v3_init_conn_apply(void* conn_ptr,
                                        unsigned char cclass) noexcept;
namespace {
constexpr unsigned char kInitDecisionBnet      = 0;
constexpr unsigned char kInitDecisionFile      = 1;
constexpr unsigned char kInitDecisionBot       = 2;
constexpr unsigned char kInitDecisionTelnet    = 3;
constexpr unsigned char kInitDecisionD2csBnetd = 4;
constexpr unsigned char kInitDecisionRejected  = 0xff;

unsigned char legacy_init_decision_for(unsigned char cclass) noexcept {
    switch (cclass) {
    case CLIENT_INITCONN_CLASS_BNET:       return kInitDecisionBnet;
    case CLIENT_INITCONN_CLASS_FILE:       return kInitDecisionFile;
    case CLIENT_INITCONN_CLASS_BOT:        return kInitDecisionBot;
    case CLIENT_INITCONN_CLASS_TELNET:     return kInitDecisionTelnet;
    case CLIENT_INITCONN_CLASS_D2CS_BNETD: return kInitDecisionD2csBnetd;
    default:                               return kInitDecisionRejected;
    }
}
}  // namespace
#endif


namespace pvpgn
{

	namespace bnetd
	{

		extern int handle_init_packet(t_connection * c, t_packet const * const packet)
		{
			if (!c)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got NULL connection", conn_get_socket(c));
				return -1;
			}
			if (!packet)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got NULL packet", conn_get_socket(c));
				return -1;
			}
			if (packet_get_class(packet) != packet_class_init)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] got bad packet (class {})", conn_get_socket(c), (int)packet_get_class(packet));
				return -1;
			}
			if ((prefs_get_max_conns_per_IP() != 0) &&
				bn_byte_get(packet->u.client_initconn.cclass) != CLIENT_INITCONN_CLASS_D2CS_BNETD &&
				(connlist_count_connections(conn_get_addr(c)) > prefs_get_max_conns_per_IP()))
			{
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] too many connections from address {} (closing connection)", conn_get_socket(c), addr_num_to_addr_str(conn_get_addr(c), conn_get_port(c)));
				return -1;
			}

			switch (packet_get_type(packet))
			{
			case CLIENT_INITCONN:
			{
				const unsigned char cclass =
					bn_byte_get(packet->u.client_initconn.cclass);

#ifdef PVPGN_V3_BNETD_INTEGRATION
				// Step 4 E.3 parity check: ask v3 what it would
				// decide for this byte and log any disagreement
				// against the legacy table below. Cheap belt-and-
				// braces guard that fires only on real drift.
				{
					unsigned char v3_decision = kInitDecisionRejected;
					pvpgn_v3_init_conn_decide(cclass, &v3_decision);
					const unsigned char legacy_decision =
						legacy_init_decision_for(cclass);
					if (v3_decision != legacy_decision)
					{
						eventlog(eventlog_level_warn, __FUNCTION__,
							"[{}] v3/legacy init dispatch mismatch "
							"(cclass 0x{:02x}: legacy={} v3={})",
							conn_get_socket(c),
							(unsigned int)cclass,
							(unsigned int)legacy_decision,
							(unsigned int)v3_decision);
					}
				}

				// Step 4 E.3 authoritative path: when the v3 apply
				// handler is installed, it transitions the
				// connection for every accepted class byte (and
				// returns -1 if a D2CS_BNETD client failed the
				// realmlist gate or `handle_d2cs_init`). When the
				// handler is NOT installed -- legacy-only build,
				// or this is being called before
				// `install_init_conn_apply_handler()` ran -- the
				// call returns 0 and the legacy switch below runs
				// unchanged.
				{
					const int v3_rc =
						pvpgn_v3_init_conn_apply(c, cclass);
					if (v3_rc == 1) break;       // v3 fully handled
					if (v3_rc == -1) return -1;  // v3 handled, failed
				}
#endif

				switch (cclass)
				{
				case CLIENT_INITCONN_CLASS_BNET:
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] client initiated bnet connection", conn_get_socket(c));
					conn_set_state(c, conn_state_connected);
					conn_set_class(c, conn_class_bnet);

					break;

				case CLIENT_INITCONN_CLASS_FILE:
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] client initiated file download connection", conn_get_socket(c));
					conn_set_state(c, conn_state_connected);
					conn_set_class(c, conn_class_file);

					break;

				case CLIENT_INITCONN_CLASS_BOT:
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] client initiated chat bot connection", conn_get_socket(c));
					conn_set_state(c, conn_state_connected);
					conn_set_class(c, conn_class_bot);

					break;

				case CLIENT_INITCONN_CLASS_TELNET:
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] client initiated telnet connection", conn_get_socket(c));
					conn_set_state(c, conn_state_connected);
					conn_set_class(c, conn_class_telnet);

					break;

				case CLIENT_INITCONN_CLASS_D2CS_BNETD:
				{
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] client initiated d2cs_bnetd connection", conn_get_socket(c));
					
					if (!(realmlist_find_realm_by_ip(conn_get_addr(c))))
					{
						eventlog(eventlog_level_info, __FUNCTION__, "[{}] d2cs connection from unknown ip address {}", conn_get_socket(c), addr_num_to_addr_str(conn_get_addr(c), conn_get_port(c)));
						return -1;
					}
					
					conn_set_state(c, conn_state_connected);
					conn_set_class(c, conn_class_d2cs_bnetd);
					if (handle_d2cs_init(c) < 0)
					{
						eventlog(eventlog_level_info, __FUNCTION__, "faild to init d2cs connection");
						return -1;
					}
				}
					break;

				case CLIENT_INITCONN_CLASS_ENC:
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] client initiated encrypted connection (not supported)", conn_get_socket(c));
					return -1;
					break;
				
				case CLIENT_INITCONN_CLASS_LOCALMACHINE:
					eventlog(eventlog_level_info, __FUNCTION__, "[{}] client initiated connection from local computer to 127.0.0.1", conn_get_socket(c));
					/*
					conn_set_state(c, conn_state_connected);
					conn_set_class(c, conn_class_localmachine;
					*/
					return -1;
					break;

				default:
					eventlog(eventlog_level_error, __FUNCTION__, "[{}] client requested unknown class 0x{:02x} (length {}) (closing connection)", conn_get_socket(c), (unsigned int)cclass, packet_get_size(packet));
					return -1;
				}
			}
				break;
			default:
				eventlog(eventlog_level_error, __FUNCTION__, "[{}] unknown init packet type 0x{:04x}, len {}", conn_get_socket(c), packet_get_type(packet), packet_get_size(packet));
				return -1;
			}

			return 0;
		}

	}

}
