// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file connection_handler.hpp
/// `IConnectionHandler` — the v3 seam between the network layer
/// (`infra::net::TcpSession`) and the protocol-handling code (legacy
/// `handle_*_packet` today, native v3 handlers later).
///
/// One handler instance is owned per session. The network layer feeds
/// raw bytes via `on_bytes` and notifies of close via `on_close`. The
/// handler pushes outbound bytes back through `IConnectionEgress`.
///
/// Lifetime: the network layer owns the handler via `unique_ptr`. The
/// handler must not survive its session.

#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

#include "core/bytes.hpp"

namespace pvpgn::application::ports {

/// Outbound channel given to the handler at construction time. The
/// handler calls `send()` to enqueue bytes for transmission and
/// `close()` to ask the network layer to disconnect.
class IConnectionEgress {
public:
    virtual ~IConnectionEgress() = default;
    virtual void send(std::vector<std::byte> bytes) = 0;
    virtual void close() = 0;
};

/// Per-session protocol handler. Implementations are stateful.
class IConnectionHandler {
public:
    virtual ~IConnectionHandler() = default;

    /// Called once before the first `on_bytes`. Gives the handler its
    /// outbound channel.
    virtual void start(IConnectionEgress& out) = 0;

    /// Called whenever the network layer has new bytes for us. The
    /// view is valid only during the call; the handler must copy
    /// anything it wants to keep.
    virtual void on_bytes(core::ByteView bytes) = 0;

    /// Called exactly once when the underlying transport closes.
    virtual void on_close() = 0;
};

/// Factory used by the acceptor: produces one handler per session.
/// The factory captures composition-root dependencies (use-cases,
/// repositories, the event bus, …).
using ConnectionHandlerFactory =
    std::function<std::unique_ptr<IConnectionHandler>()>;

}  // namespace pvpgn::application::ports
