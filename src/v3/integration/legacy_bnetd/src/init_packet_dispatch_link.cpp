// SPDX-License-Identifier: GPL-2.0-or-later
//
// R176.d: relocated from `src/bnetd/handle_init.cpp`.
// R186.a: bridge-free implementation. `handle_init_packet` now
// drives the v3 `PacketPumpDriver` directly with the linked
// `InitSideEffects` table -- no more round-trip through
// `pvpgn_v3_init_conn_apply_ex` / `apply_via_legacy`. The driver
// is the SOLE authority for the init verdict and the side
// effects flow through the function-pointer port defined in
// `application/bnet_packet_pump/init_side_effects.hpp`.
//
// The four pre-existing validation gates (NULL conn / NULL
// packet / packet class != init / packet type != CLIENT_INITCONN)
// stay -- they live above the driver because they read the
// legacy `t_packet` shape that the driver intentionally does not
// know about.

#include "application/bnet_packet_pump/driver.hpp"
#include "application/bnet_packet_pump/init_side_effects.hpp"
#include "integration/legacy_bnetd/init_side_effects_link.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

#include "common/setup_before.h"

#include "common/init_protocol.h"
#include "common/eventlog.h"
#include "common/packet.h"
#include "common/bn_type.h"

#include "prefs_v3_shim.h"
#include "connection.h"
#include "realm.h"

#include "common/setup_after.h"

namespace pvpgn {
namespace bnetd {

extern int handle_init_packet(t_connection* c, t_packet const* const packet)
{
    if (!c) {
        eventlog(eventlog_level_error, __FUNCTION__, "got NULL connection");
        return -1;
    }
    if (!packet) {
        eventlog(eventlog_level_error, __FUNCTION__,
                 "[{}] got NULL packet", conn_get_socket(c));
        return -1;
    }
    if (packet_get_class(packet) != packet_class_init) {
        eventlog(eventlog_level_error, __FUNCTION__,
                 "[{}] got bad packet (class {})",
                 conn_get_socket(c),
                 static_cast<int>(packet_get_class(packet)));
        return -1;
    }
    if (packet_get_type(packet) != CLIENT_INITCONN) {
        eventlog(eventlog_level_error, __FUNCTION__,
                 "[{}] unknown init packet type 0x{:04x}, len {}",
                 conn_get_socket(c),
                 packet_get_type(packet),
                 packet_get_size(packet));
        return -1;
    }

    unsigned char const cclass =
        bn_byte_get(packet->u.client_initconn.cclass);

    namespace pump = ::pvpgn::application::bnet_packet_pump;

    pump::PumpPolicy pol{};
    pol.conn_count       = connlist_count_connections(conn_get_addr(c));
    pol.max_conns_per_ip = prefs_v3::max_conns_per_IP();
    pol.d2cs_ip_allowed  = realmlist_find_realm_by_ip(conn_get_addr(c)) != nullptr;

    std::array<std::byte, 1> const frame{ static_cast<std::byte>(cclass) };

    pump::PacketPumpDriver driver{};
    auto const outcome = driver.feed_with_side_effects(
        frame, pol,
        static_cast<void*>(c),
        ::pvpgn::integration::legacy_bnetd::get_legacy_init_side_effects());

    if (outcome != pump::FeedOutcome::kAccepted) {
        eventlog(eventlog_level_info, __FUNCTION__,
                 "[{}] v3 driver rejected init for cclass 0x{:02x} (outcome {})",
                 conn_get_socket(c),
                 static_cast<unsigned>(cclass),
                 pump::to_string(outcome));
        return -1;
    }
    return 0;
}

}  // namespace bnetd
}  // namespace pvpgn
