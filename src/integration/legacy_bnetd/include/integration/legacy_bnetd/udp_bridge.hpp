// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file udp_bridge.hpp
/// Composition root for the bnetd → v3 UDP strangler-fig cut.
///
/// Owns:
///   * an `infra::net::IoRuntime` running on its own thread,
///   * one `infra::net::UdpEndpoint` per adopted file descriptor,
///   * one `LegacyUdpDispatcher` wiring each endpoint to
///     `pvpgn::bnetd::handle_udp_packet`.
///
/// Lifecycle (driven from `bnetd/main.cpp`):
///
///   1. Before pre-server-startup, call
///      `pvpgn::bnetd::server_set_skip_legacy_udp_fdwatch(true)`
///      so the legacy server opens the UDP sockets but skips
///      registering them with `fdwatch`.
///   2. After pre-server-startup completes, call `install()` on
///      the bridge. It enumerates the reserved fds via
///      `pvpgn::bnetd::server_get_bnet_udp_fds()` and adopts each.
///   3. The legacy server enters its TCP-only fdwatch loop.
///   4. On shutdown, before legacy `post_server_shutdown`, call
///      `shutdown()` to stop the io thread and close endpoints.
///
/// Available only in the combined build (`PVPGN_BUILD_LEGACY=ON`
/// + `PVPGN_BUILD_V3=ON` + `PVPGN_V3_WITH_BOOST=ON`).

#include <memory>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"

namespace pvpgn::infra::net { class IoRuntime; }

namespace pvpgn::integration::legacy_bnetd {

class UdpBridgeImpl;

class UdpBridge {
public:
    /// Default-constructed bridges own their own `IoRuntime` and io
    /// worker thread.  Useful for tests and standalone wiring.
    UdpBridge();

    /// Share an externally-owned `IoRuntime`.  The bridge will
    /// schedule its endpoints on @p runtime and rely on the owner to
    /// `run()`/`stop()` it.  @p runtime must outlive this bridge.
    explicit UdpBridge(infra::net::IoRuntime& runtime);

    ~UdpBridge();

    UdpBridge(const UdpBridge&)            = delete;
    UdpBridge& operator=(const UdpBridge&) = delete;
    UdpBridge(UdpBridge&&)                 = delete;
    UdpBridge& operator=(UdpBridge&&)      = delete;

    /// Adopt the UDP fds reserved by the legacy server, start the
    /// io thread, and begin draining datagrams into the legacy
    /// `handle_udp_packet`.
    ///
    /// @returns the number of endpoints that were successfully
    ///          installed, or an error if no fds were available
    ///          or if the io thread could not be started.
    core::Result<std::size_t> install();

    /// Stop reception, close endpoints, join the io thread.
    /// Idempotent. Called automatically by the destructor.
    void shutdown();

    /// Number of adopted endpoints (0 before install()).
    std::size_t endpoint_count() const noexcept;

private:
    std::unique_ptr<UdpBridgeImpl> impl_;
};

}  // namespace pvpgn::integration::legacy_bnetd
