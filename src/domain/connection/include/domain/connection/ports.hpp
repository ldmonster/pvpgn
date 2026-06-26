// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
//
// domain/connection/ports.hpp — Abstract ports (interfaces) for the connection bounded context.
// Implementations live in src/infra/<tech>/ and src/integration/<binding>/.

#include <cstddef>
#include <span>
#include <utility>
#include <vector>

#include "core/bytes.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::domain::connection {

// ---------------------------------------------------------------------------
// IConnectionEgress
// ---------------------------------------------------------------------------

/// Egress side of a single client connection. Implementations forward
/// the supplied bytes to the underlying transport (TCP, mock, …).
class IConnectionEgress {
public:
    virtual ~IConnectionEgress() = default;

    IConnectionEgress(const IConnectionEgress&)            = delete;
    IConnectionEgress& operator=(const IConnectionEgress&) = delete;
    IConnectionEgress(IConnectionEgress&&)                 = delete;
    IConnectionEgress& operator=(IConnectionEgress&&)      = delete;

    /// Enqueue @p bytes for asynchronous transmission. The implementation
    /// takes ownership and is responsible for honouring write ordering.
    virtual void send(std::vector<std::byte> bytes) = 0;

    /// Initiate graceful close of the underlying connection. Pending
    /// queued bytes should still be flushed when possible.
    virtual void close() = 0;

protected:
    IConnectionEgress() = default;
};

// ---------------------------------------------------------------------------
// IConnectionHandler
// ---------------------------------------------------------------------------

/// Ingress side of a single client connection. Implementations consume
/// inbound bytes, decode framed messages, and write responses via the
/// `IConnectionEgress` supplied at `start()`.
class IConnectionHandler {
public:
    virtual ~IConnectionHandler() = default;

    IConnectionHandler(const IConnectionHandler&)            = delete;
    IConnectionHandler& operator=(const IConnectionHandler&) = delete;
    IConnectionHandler(IConnectionHandler&&)                 = delete;
    IConnectionHandler& operator=(IConnectionHandler&&)      = delete;

    /// Invoked once when the connection is ready. The egress reference
    /// must outlive the handler's use of it.
    virtual void start(IConnectionEgress& egress) = 0;

    /// Feed inbound bytes from the transport into the handler.
    virtual void on_bytes(core::ByteView bytes) = 0;

    /// Notify the handler that the peer has closed the connection.
    virtual void on_close() = 0;

protected:
    IConnectionHandler() = default;
};

// ---------------------------------------------------------------------------
// IMessageRouter
// ---------------------------------------------------------------------------

class IMessageRouter {
public:
    virtual ~IMessageRouter() = default;

    IMessageRouter(const IMessageRouter&)            = delete;
    IMessageRouter& operator=(const IMessageRouter&) = delete;
    IMessageRouter(IMessageRouter&&)                 = delete;
    IMessageRouter& operator=(IMessageRouter&&)      = delete;

    virtual core::Result<void, core::Error>
    send(domain::SessionId session_id, std::span<const std::byte> bytes) = 0;

    virtual core::Result<void, core::Error>
    broadcast(std::span<const domain::SessionId> sessions,
              std::span<const std::byte> bytes) = 0;

    virtual core::Result<void, core::Error>
    send_to_account(domain::AccountId account_id,
                    std::span<const std::byte> bytes) = 0;

    /// Forcibly close the connection registered under @p session_id (e.g.
    /// kick-old-login). Default: no-op, so test fakes need not implement it.
    virtual core::Result<void, core::Error>
    disconnect(domain::SessionId /*session_id*/) {
        return core::ok();
    }

protected:
    IMessageRouter() = default;
};

} // namespace pvpgn::domain::connection
