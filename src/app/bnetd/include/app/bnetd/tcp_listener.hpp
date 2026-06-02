// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file tcp_listener.hpp
/// `TcpListener` — thin wrapper around `infra::net::TcpAcceptor` that
/// binds a port and creates protocol-specific sessions for each accepted
/// connection.
///
/// Design
/// ------
/// `TcpListener` is a value type that owns one `TcpAcceptor`. The caller
/// supplies a `SessionFactory` callback that receives the accepted
/// `shared_ptr<TcpSession>` and is responsible for:
///   1. Creating the appropriate egress adapter (`TcpSessionEgress`).
///   2. Creating the FSM context (`BnetSessionContextImpl`, etc.).
///   3. Creating the FSM (`BnetFsm`, `BnftpFsm`, `WolFsm`).
///   4. Wiring `tcp_session->set_on_bytes` / `set_on_close`.
///   5. Calling `tcp_session->start()`.
///
/// The composition root in `main.cpp` creates one `TcpListener` per
/// protocol port and stores them in a `std::vector` for the lifetime of
/// the server.
///
/// Thread-safety
/// -------------
/// `start()` and `stop()` must be called from the same thread (or with
/// external synchronisation). The `SessionFactory` callback is invoked
/// from an Asio worker thread; it must be thread-safe.

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "infra/net/io_runtime.hpp"
#include "infra/net/tcp_acceptor.hpp"
#include "infra/net/tcp_session.hpp"

namespace pvpgn::app::bnetd {

/// Listens on a TCP port and creates sessions via a user-supplied factory.
class TcpListener {
public:
    using SessionFactory =
        std::function<void(std::shared_ptr<infra::net::TcpSession>)>;

    /// Construct a listener.
    /// @param rt           Shared Asio runtime (io_context + thread pool).
    /// @param factory      Called for each accepted connection.
    /// @param idle_timeout Idle-read deadline applied to every accepted
    ///   session *before* the factory runs (so the factory's `start()`
    ///   honours it). Zero (the default) disables the timeout. Sourced
    ///   from `[net.timeouts]` (Plan 06).
    TcpListener(infra::net::IoRuntime& rt, SessionFactory factory,
                std::chrono::milliseconds idle_timeout =
                    std::chrono::milliseconds::zero())
        : acceptor_(rt,
                    [factory = std::move(factory), idle_timeout]
                    (std::shared_ptr<infra::net::TcpSession> s) {
                        if (idle_timeout > std::chrono::milliseconds::zero()) {
                            s->set_idle_timeout(idle_timeout);
                        }
                        factory(std::move(s));
                    }) {}

    TcpListener(const TcpListener&)            = delete;
    TcpListener& operator=(const TcpListener&) = delete;
    TcpListener(TcpListener&&)                 = delete;
    TcpListener& operator=(TcpListener&&)      = delete;

    ~TcpListener() { stop(); }

    /// Bind to `address:port` and begin accepting connections.
    /// Returns the bound endpoint on success.
    /// Throws `std::runtime_error` on bind/listen failure.
    void start(const std::string& address, std::uint16_t port) {
        auto result = acceptor_.listen(address, port);
        if (!result) {
            throw std::runtime_error(
                "TcpListener: failed to bind " + address + ":" +
                std::to_string(port) + " — " + result.error().message());
        }
    }

    /// Stop accepting new connections. Existing sessions are unaffected.
    void stop() { acceptor_.close(); }

private:
    infra::net::TcpAcceptor acceptor_;
};

}  // namespace pvpgn::app::bnetd
