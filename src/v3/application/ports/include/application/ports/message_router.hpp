// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file message_router.hpp
/// Protocol-agnostic outbound message routing interface.
/// Routes encoded bytes to sessions identified by SessionId.
/// Used by protocol handlers to send bytes back to clients.

#include <cstddef>
#include <span>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::ports {

/// Protocol-agnostic message router.
/// Routes pre-encoded bytes to one or more sessions by SessionId.
/// Also supports lookup-and-send by AccountId (internal use).
class IMessageRouter {
public:
    virtual ~IMessageRouter() = default;

    /// Send pre-encoded bytes to a single session.
    /// Returns an error if the session is not found or send fails.
    virtual core::Result<void, core::Error>
    send(domain::SessionId session_id, std::span<const std::byte> bytes) = 0;

    /// Broadcast pre-encoded bytes to multiple sessions.
    /// The buffer is shared across all recipients (zero-copy).
    /// Returns an error if any send fails (best-effort).
    virtual core::Result<void, core::Error> broadcast(
        std::span<const domain::SessionId> sessions,
        std::span<const std::byte> bytes) = 0;

    /// Send bytes to the session for a given account.
    /// Performs SessionId lookup via ISessionRegistry, then sends.
    /// Returns an error if account has no session or send fails.
    virtual core::Result<void, core::Error>
    send_to_account(domain::AccountId account_id,
                    std::span<const std::byte> bytes) = 0;
};

}  // namespace pvpgn::application::ports
