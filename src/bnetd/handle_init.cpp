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
 *
 * R170.e: this file was the original legacy init-connection
 * handler -- a 230-line cclass switch that the server.cpp packet
 * pump fanned out to. R166-R169 stranglered the entire decision
 * tree behind `pvpgn_v3_init_conn_apply_ex` in
 * `integration_legacy_bnetd`, which forwards to the
 * `application/init/init_conn_dispatch` use case. After R169.b the
 * legacy switch is gated under `!PVPGN_V3_BNETD_INTEGRATION`;
 * R170.e collapses the v3 path to a thin shim that just calls the
 * bridge and returns. The full removal of `handle_init_packet` (and
 * the file itself) is gated on retiring the legacy server.cpp
 * packet pump -- tracked in plans/phase3b-handle-bnet-audit.md.
 */
#include "common/setup_before.h"
#include "handle_init.h"

#include "common/init_protocol.h"
#include "common/eventlog.h"
#include "common/packet.h"
#include "common/bn_type.h"
#include "common/addr.h"

#include "prefs_v3_shim.h"
#include "connection.h"
#include "realm.h"
#include "handle_d2cs.h"
#include "common/setup_after.h"

#ifdef PVPGN_V3_BNETD_INTEGRATION
// R169.a authoritative apply hook. The bridge resolves
// `pvpgn_v3_init_conn_apply_ex` -> `dispatch_init_conn` and applies
// the chosen state transition on the legacy connection object.
// Return values: 1 = v3 handled the connection, 0 = decline (only
// returned when conn_ptr is NULL -- guarded above), -1 = explicit
// reject (rate-limited / D2CS realmlist deny / no handler installed).
extern "C" int pvpgn_v3_init_conn_apply_ex(void* conn_ptr,
                                           unsigned char cclass,
                                           unsigned int conn_count,
                                           unsigned int max_conns_per_ip,
                                           int d2cs_ip_allowed) noexcept;
#endif


namespace pvpgn
{

namespace bnetd
{

extern int handle_init_packet(t_connection * c, t_packet const * const packet)
{
if (!c)
{
eventlog(eventlog_level_error, __FUNCTION__, "got NULL connection");
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

if (packet_get_type(packet) != CLIENT_INITCONN)
{
eventlog(eventlog_level_error, __FUNCTION__, "[{}] unknown init packet type 0x{:04x}, len {}", conn_get_socket(c), packet_get_type(packet), packet_get_size(packet));
return -1;
}

const unsigned char cclass =
bn_byte_get(packet->u.client_initconn.cclass);

#ifdef PVPGN_V3_BNETD_INTEGRATION
// R170.e shim: the v3 dispatcher is authoritative for
// every accepted cclass byte. Rate-limit, realmlist gate,
// class dispatch, and handle_d2cs_init are all owned by
// `pvpgn_v3_init_conn_apply_ex`. Return 1 = handled OK,
// -1 = rejected for any reason (caller closes the conn).
const int v3_rc = pvpgn_v3_init_conn_apply_ex(
c, cclass,
connlist_count_connections(conn_get_addr(c)),
prefs_v3::max_conns_per_IP(),
realmlist_find_realm_by_ip(conn_get_addr(c)) != nullptr ? 1 : 0);
return (v3_rc == 1) ? 0 : -1;
#else
// Non-v3 builds (PVPGN_BUILD_V3=OFF): the legacy cclass
// switch is no longer supported -- bnetd_legacy depends
// on prefs_v3_shim.h, which itself requires the v3
// bridge. If you reach this branch, your CMake config is
// inconsistent.
#error "handle_init.cpp now requires PVPGN_V3_BNETD_INTEGRATION; PVPGN_BUILD_V3=OFF is no longer supported for bnetd_legacy."
#endif
}

}

}