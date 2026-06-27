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
