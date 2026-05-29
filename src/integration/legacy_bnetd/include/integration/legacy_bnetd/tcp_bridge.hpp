// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file tcp_bridge.hpp
/// Composition root for the bnetd -> v3 TCP-accept strangler-fig cut.
///
/// Owns:
///   * an `infra::net::IoRuntime` running on its own thread (shared
///     concept with `UdpBridge` -- each bridge has its own runtime
///     today; consolidating is a follow-up),
///   * one `infra::net::TcpAcceptor` per adopted bnet TCP listening
///     socket.
///
/// On each accepted socket the bridge releases the native handle
/// from Asio and hands it back to legacy bnetd via
/// `pvpgn::bnetd::server_handle_v3_accepted_bnet_socket()`, which
/// runs the post-accept setup (ipban check, SO_KEEPALIVE,
/// getsockname, non-blocking, `conn_create`, `conn_add_fdwatch`).
/// Legacy retains ownership of the per-connection read loop -- this
/// bridge only replaces the accept syscall.
///
/// Lifecycle (driven from `bnetd/main.cpp`, gated on
/// `PVPGN_V3_BNETD_INTEGRATION`):
///
///   1. Before `pre_server_startup`, call
///      `pvpgn::bnetd::server_set_skip_legacy_tcp_fdwatch(true)`
///      so the legacy server opens the bnet TCP listening sockets
///      and puts them into `listen()` state but skips registering
///      them with the legacy fdwatch loop.
///   2. From the `server_after_setup_hook`, call `install()` on
///      this bridge. It enumerates listeners via
///      `pvpgn::bnetd::server_get_bnet_tcp_listeners()` and adopts
///      each TCP fd into an Asio acceptor.
///   3. The bridge accepts incoming connections on a v3 io thread
///      and forwards each to legacy via
///      `server_handle_v3_accepted_bnet_socket()`.
///   4. From the `server_before_shutdown_hook`, call `shutdown()`.
///
/// Only bnet TCP listeners are affected. IRC / WOL / telnet /
/// w3route / wgameres / apireg listeners still use the legacy
/// fdwatch accept loop.
///
/// Available only in the combined build (`PVPGN_BUILD_LEGACY=ON`
/// + `PVPGN_BUILD_V3=ON` + `PVPGN_V3_WITH_BOOST=ON`).

#include <memory>

#include "core/error.hpp"
#include "core/result.hpp"

namespace pvpgn::infra::net { class IoRuntime; }

namespace pvpgn::integration::legacy_bnetd {

class TcpBridgeImpl;

class TcpBridge {
public:
    /// Default-constructed bridges own their own `IoRuntime` and io
    /// worker thread.  Useful for tests and standalone wiring.
    TcpBridge();

    /// Share an externally-owned `IoRuntime`.  The bridge will
    /// schedule its acceptors on @p runtime and rely on the owner to
    /// `run()`/`stop()` it.  @p runtime must outlive this bridge.
    explicit TcpBridge(infra::net::IoRuntime& runtime);

    ~TcpBridge();

    TcpBridge(const TcpBridge&)            = delete;
    TcpBridge& operator=(const TcpBridge&) = delete;
    TcpBridge(TcpBridge&&)                 = delete;
    TcpBridge& operator=(TcpBridge&&)      = delete;

    /// Adopt the bnet TCP listening sockets reserved by the legacy
    /// server, start the io thread, and begin accepting.
    ///
    /// @returns the number of acceptors successfully installed, or
    ///          an error if no listeners were available or the io
    ///          thread could not start.
    core::Result<std::size_t> install();

    /// Stop accepting, close acceptors (releasing fds back to
    /// legacy first), join the io thread. Idempotent. Called
    /// automatically by the destructor.
    void shutdown();

    /// Number of adopted acceptors (0 before `install()`).
    std::size_t acceptor_count() const noexcept;

private:
    std::unique_ptr<TcpBridgeImpl> impl_;
};

}  // namespace pvpgn::integration::legacy_bnetd
