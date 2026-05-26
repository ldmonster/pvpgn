// SPDX-License-Identifier: GPL-2.0-or-later
//
// Linked-variant dispatch hook for `LegacyBnetFrameRouter`
// (Batch 38b).  Installed at static-init time so that any code
// that creates a `LegacyBnetFrameRouter` after `bnetd_legacy` is
// loaded gets the real `handle_bnet_packet` -> outqueue-drain
// behaviour without explicit composition-root wiring.
//
// Build-only: this TU compiles + links the seam, but nothing in
// `src/bnetd/` calls into `LegacyBnetFrameRouter` yet.  The
// accept-path flip lands in Batch 38c behind a `bnetd.conf`
// flag.
//
// Lifetime: `g_register` runs after dynamic init of the legacy
// globals it depends on (this TU is in the same static lib chain
// as `bnetd_legacy`, but `handle_bnet_packet` itself does not
// reach into any constructor-initialised state -- it walks the
// packet bytes that we just filled in).

#include <cstddef>
#include <cstring>
#include <span>
#include <vector>

#include "application/ports/connection_handler.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"
#include "integration/legacy_bnetd/legacy_bnet_frame_router.hpp"

#include "common/setup_before.h"
#include "common/packet.h"
#include "bnetd/connection.h"
#include "bnetd/handle_bnet.h"
#include "common/setup_after.h"

// Forward decl: `handle_init_packet` is defined in
// `init_packet_dispatch_link.cpp` (same static lib). The retired
// `bnetd/handle_init.h` header (R178.b) used to provide this.
namespace pvpgn { namespace bnetd {
    extern int handle_init_packet(t_connection* c, t_packet const* const packet);
}}

namespace pvpgn::integration::legacy_bnetd {

namespace {

constexpr std::size_t kMaxPacketSize = 3072;  // legacy MAX_PACKET_SIZE

bool dispatch_via_legacy(LegacyBnetConnection*           opaque_conn,
                         std::span<const std::byte>      frame,
                         application::ports::IConnectionEgress& egress) noexcept {
    if (opaque_conn == nullptr) return false;
    if (frame.empty() || frame.size() > kMaxPacketSize) return false;

    auto* conn = reinterpret_cast<::pvpgn::bnetd::t_connection*>(opaque_conn);

    // Build a t_packet of class `bnet` containing the framed bytes
    // (header included). This mirrors the on-wire shape that
    // `_handle_connection` would have produced when reading from
    // fdwatch, so `handle_bnet_packet` sees exactly the same
    // packet layout.
    auto* packet = ::pvpgn::packet_create(::pvpgn::packet_class_bnet);
    if (packet == nullptr) return false;

    void* raw = ::pvpgn::packet_get_raw_data_build(packet, 0);
    if (raw == nullptr) {
        ::pvpgn::packet_del_ref(packet);
        return false;
    }
    std::memcpy(raw, frame.data(), frame.size());
    if (::pvpgn::packet_set_size(packet,
                                 static_cast<unsigned int>(frame.size())) < 0) {
        ::pvpgn::packet_del_ref(packet);
        return false;
    }

    // Dispatch to the legacy handler that matches the connection's
    // current class. New accepted bnet sockets start in
    // `conn_class_init`; the first byte (magic) is consumed by
    // `handle_init_packet`, which then transitions the conn to the
    // real class (typically `conn_class_bnet`). After that, the
    // class-refresh hook below mirrors the new class on the v3
    // side so subsequent frames are parsed with the correct
    // framing.
    int rc;
    switch (::pvpgn::bnetd::conn_get_class(conn)) {
        case ::pvpgn::bnetd::conn_class_init:
            rc = ::pvpgn::bnetd::handle_init_packet(conn, packet);
            break;
        case ::pvpgn::bnetd::conn_class_bnet:
            rc = ::pvpgn::bnetd::handle_bnet_packet(conn, packet);
            break;
        default:
            bridge_log(core::LogLevel::Warn, "v3.bnet_router",
                       "frame arrived for unsupported conn class; "
                       "v3 TcpSession path currently handles only "
                       "init + bnet classes");
            rc = -1;
            break;
    }
    ::pvpgn::packet_del_ref(packet);

    if (rc < 0) {
        // Legacy returns -1 to request connection teardown; surface
        // that by closing the egress. The v3 session will see the
        // close and unwind accordingly.
        bridge_log(core::LogLevel::Debug, "v3.bnet_router",
                   "handle_bnet_packet returned -1; closing egress");
        egress.close();
        // Treat as handled even though it failed: legacy owned the
        // dispatch and decided to drop the conn.
        return true;
    }

    // Drain the outqueue. handle_bnet_packet (and anything it
    // synchronously called) may have pushed any number of reply
    // packets via `conn_push_outqueue`. Pull them all and forward
    // their raw bytes through the v3 egress port.
    while (auto* out = ::pvpgn::bnetd::conn_pull_outqueue(conn)) {
        const unsigned int sz = ::pvpgn::packet_get_size(out);
        if (sz == 0 || sz > kMaxPacketSize) {
            ::pvpgn::packet_del_ref(out);
            continue;
        }
        auto const* src = static_cast<std::byte const*>(
            ::pvpgn::packet_get_raw_data_const(out, 0));
        std::vector<std::byte> bytes(sz);
        std::memcpy(bytes.data(), src, sz);
        egress.send(std::move(bytes));
        ::pvpgn::packet_del_ref(out);
    }

    return true;
}

// 38c: extern "C"-ish redirect callback installed via
// `conn_install_v3_outbound_route`. Receives the opaque
// `LegacyBnetFrameRouter*` previously stashed on the conn via
// `conn_set_v3_router`, plus the raw outbound packet bytes.
// Returns non-zero on success (legacy queue is skipped); 0 on
// failure (legacy queue is used as fallback).
int route_outbound_via_router(void*        router_ptr,
                              void const*  bytes,
                              unsigned int size) noexcept {
    if (router_ptr == nullptr || bytes == nullptr || size == 0) return 0;
    auto* router = static_cast<LegacyBnetFrameRouter*>(router_ptr);
    const std::span<const std::byte> view{
        static_cast<std::byte const*>(bytes),
        static_cast<std::size_t>(size)};
    return router->send_outbound(view) ? 1 : 0;
}

// 38f: mirror legacy `conn_get_class()` to the v3 `ConnectionClass`
// enum so the router can switch framing after `handle_init_packet`
// transitions a freshly accepted conn from `conn_class_init` to its
// real class (typically `conn_class_bnet`).
ConnectionClass refresh_class_from_conn(
    LegacyBnetConnection* opaque_conn) noexcept {
    if (opaque_conn == nullptr) return ConnectionClass::Init;
    auto* conn = reinterpret_cast<::pvpgn::bnetd::t_connection*>(opaque_conn);
    switch (::pvpgn::bnetd::conn_get_class(conn)) {
        case ::pvpgn::bnetd::conn_class_init:        return ConnectionClass::Init;
        case ::pvpgn::bnetd::conn_class_bnet:        return ConnectionClass::Bnet;
        case ::pvpgn::bnetd::conn_class_file:        return ConnectionClass::File;
        case ::pvpgn::bnetd::conn_class_d2cs_bnetd:  return ConnectionClass::D2csBnetd;
        case ::pvpgn::bnetd::conn_class_w3route:     return ConnectionClass::W3route;
        case ::pvpgn::bnetd::conn_class_wgameres:    return ConnectionClass::WolGameres;
        case ::pvpgn::bnetd::conn_class_bot:         return ConnectionClass::Bot;
        case ::pvpgn::bnetd::conn_class_telnet:      return ConnectionClass::Telnet;
        case ::pvpgn::bnetd::conn_class_ircinit:
        case ::pvpgn::bnetd::conn_class_irc:         return ConnectionClass::Irc;
        case ::pvpgn::bnetd::conn_class_wol:
        case ::pvpgn::bnetd::conn_class_wserv:       return ConnectionClass::Wol;
        case ::pvpgn::bnetd::conn_class_wladder:     return ConnectionClass::Wladder;
        default:                                     return ConnectionClass::Bnet;
    }
}

struct AutoRegister {
    AutoRegister() noexcept {
        LegacyBnetFrameRouter::set_dispatch_hook(&dispatch_via_legacy);
        LegacyBnetFrameRouter::set_class_refresh(&refresh_class_from_conn);
        // 38c: also install the conn_push_outqueue redirect so
        // legacy code that pushes packets asynchronously
        // (timers, channel broadcasts, /whisper reply, etc.)
        // reaches the v3 egress instead of the fdwatch writer.
        ::pvpgn::bnetd::conn_install_v3_outbound_route(
            &route_outbound_via_router);
    }
} g_register;

}  // namespace

}  // namespace pvpgn::integration::legacy_bnetd
