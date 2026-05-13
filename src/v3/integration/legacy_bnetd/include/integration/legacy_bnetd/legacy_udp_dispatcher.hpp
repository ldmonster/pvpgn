// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file legacy_udp_dispatcher.hpp
/// Bridges `infra::net::UdpEndpoint` to the legacy
/// `pvpgn::bnetd::handle_udp_packet`. UDP is the easiest legacy
/// hand-off because the handler does not require a `t_connection*`.
///
/// **Composition-root contract.** This dispatcher *links* the legacy
/// bnetd library, but the legacy code reaches into a forest of
/// global state (prefs, eventlog, account list, ladder, …). It is
/// the caller's responsibility to have run the legacy initialisation
/// path before any datagram arrives. Constructing the dispatcher is
/// safe and side-effect free; calling `start()` is not.
///
/// Available only when both `PVPGN_BUILD_LEGACY=ON` and
/// `PVPGN_BUILD_V3=ON` so the v3 build can stand alone in CI.

#include <cstdint>

namespace pvpgn::infra::net { class UdpEndpoint; }

namespace pvpgn::integration::legacy_bnetd {

class LegacyUdpDispatcher {
public:
    /// @param ep The bound UDP endpoint to drain. Must outlive the
    ///   dispatcher.
    /// @param legacy_socket_fd The OS file descriptor the legacy code
    ///   uses as the "udp socket" identifier. The legacy
    ///   `handle_udp_packet` only uses it for outbound replies; if
    ///   the dispatcher owns reply transport, pass any non-negative
    ///   integer (it gets echoed into legacy logging).
    LegacyUdpDispatcher(infra::net::UdpEndpoint& ep,
                        int legacy_socket_fd) noexcept;

    /// Wire the receive callback. Idempotent.
    void start();

private:
    infra::net::UdpEndpoint& ep_;
    int                      legacy_socket_fd_;
    bool                     started_ = false;
};

}  // namespace pvpgn::integration::legacy_bnetd
