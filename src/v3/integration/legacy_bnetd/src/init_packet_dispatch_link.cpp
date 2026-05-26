// SPDX-License-Identifier: GPL-2.0-or-later
//
// R176.d: relocated from `src/bnetd/handle_init.cpp`.
//
// `handle_init_packet` is now defined inside the integration-
// linked half of v3 so that retiring it from `bnetd_legacy` is a
// CMake-only switch in future rounds. Externally the symbol is
// indistinguishable from the previous definition: same
// `pvpgn::bnetd::` namespace, same signature, same return values.
//
// The body itself is a thin shim over `pvpgn_v3_init_conn_apply_ex`
// (the R169.a authoritative apply hook). All real init-class
// decisions live in `application/init/init_conn_dispatch`; this
// file only does the four cheap validation gates (NULL conn /
// NULL packet / packet class != init / packet type != CLIENT_INITCONN)
// and forwards the cclass byte plus connection-population
// counters into the bridge.

// R181.c: shadow-trace the init byte through the pure-C++ v3
// `PacketPumpDriver` so the upcoming packet-pump arc (R182+) can
// be validated against the legacy `pvpgn_v3_init_conn_apply_ex`
// verdict in real traffic without changing the runtime contract.
// Driver outcome is purely advisory at this stage -- logged only
// when it disagrees with the legacy authoritative verdict.
#include "application/bnet_packet_pump/driver.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

#include "common/setup_before.h"

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
extern "C" int pvpgn_v3_init_conn_apply_ex(void* conn_ptr,
                                           unsigned char cclass,
                                           unsigned int conn_count,
                                           unsigned int max_conns_per_ip,
                                           int d2cs_ip_allowed) noexcept;
#endif

namespace {

// Feed one cclass byte through a fresh v3 `PacketPumpDriver` with
// the same policy inputs the authoritative legacy/v3 path will
// use, and return the resulting outcome. Pure / stack-only.
::pvpgn::application::bnet_packet_pump::FeedOutcome
pump_observe_cclass(unsigned char cclass,
                    unsigned int  conn_count,
                    unsigned int  max_conns_per_ip,
                    bool          d2cs_ip_allowed) noexcept
{
    namespace pump = ::pvpgn::application::bnet_packet_pump;
    pump::PacketPumpDriver d{};
    pump::PumpPolicy pol{};
    pol.conn_count       = conn_count;
    pol.max_conns_per_ip = max_conns_per_ip;
    pol.d2cs_ip_allowed  = d2cs_ip_allowed;
    std::array<std::byte, 1> const one{ static_cast<std::byte>(cclass) };
    return d.feed(one, pol);
}

}  // namespace

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

    unsigned int const conn_count       = connlist_count_connections(conn_get_addr(c));
    unsigned int const max_conns_per_ip = prefs_v3::max_conns_per_IP();
    bool         const d2cs_ip_allowed  = realmlist_find_realm_by_ip(conn_get_addr(c)) != nullptr;

    // R184.d: driver owns the rejection path end-to-end. When the
    // v3 `PacketPumpDriver` rejects (unknown cclass, rate-limit
    // exceeded, denied D2CS IP), we return `-1` immediately
    // WITHOUT touching the legacy bridge -- both paths consult
    // `application/init/dispatch_init_conn` against identical
    // policy inputs, so the bridge would just return 0 / -1 with
    // no side effects on `t_connection`. Skipping it shrinks the
    // hot path on rejections and is the first concrete cut at
    // the legacy bridge's responsibilities. The accept path
    // still calls the bridge for its side effects
    // (`conn_set_state`, `conn_set_class`, `handle_d2cs_init`,
    // response packets) -- those moves are R185+.
    auto const pump_outcome = pump_observe_cclass(cclass,
                                                  conn_count,
                                                  max_conns_per_ip,
                                                  d2cs_ip_allowed);

    using ::pvpgn::application::bnet_packet_pump::FeedOutcome;
    if (pump_outcome != FeedOutcome::kAccepted) {
        eventlog(eventlog_level_info, __FUNCTION__,
                 "[{}] v3 driver rejected init for cclass 0x{:02x} (outcome {})",
                 conn_get_socket(c),
                 static_cast<unsigned>(cclass),
                 ::pvpgn::application::bnet_packet_pump::to_string(pump_outcome));
        return -1;
    }

#ifdef PVPGN_V3_BNETD_INTEGRATION
    // Accept path: bridge still owns the side effects.
    int const v3_rc = pvpgn_v3_init_conn_apply_ex(
        c, cclass,
        conn_count,
        max_conns_per_ip,
        d2cs_ip_allowed ? 1 : 0);
    int const legacy_rc = (v3_rc == 1) ? 0 : -1;

    // Parity tripwire on the accept path: if the bridge somehow
    // disagrees after the driver accepted, that's a real divergence
    // (side-effect-only handler returning -1, e.g.
    // `handle_d2cs_init` failed). We surface it but trust the
    // bridge's verdict here -- the side-effecting handler is
    // still the source of truth for "did the connection actually
    // come up".
    if (legacy_rc != 0) {
        eventlog(eventlog_level_warn, __FUNCTION__,
                 "[{}] v3 driver accepted cclass 0x{:02x} but legacy bridge "
                 "rejected (side-effect failure?) -- trusting bridge",
                 conn_get_socket(c),
                 static_cast<unsigned>(cclass));
    }
    return legacy_rc;
#else
#error "init_packet_dispatch_link.cpp now requires PVPGN_V3_BNETD_INTEGRATION; PVPGN_BUILD_V3=OFF is no longer supported for bnetd_legacy."
#endif
}

}  // namespace bnetd
}  // namespace pvpgn
