// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file session_context.hpp
/// Dependency surface of the IRC FSM. Concrete sessions plug in the
/// transport and (in Phase 5) the use-case bus.

#include "core/result.hpp"
#include "protocol/irc/message.hpp"

namespace pvpgn::protocol::irc {

class ISessionContext {
public:
    virtual ~ISessionContext() = default;

    /// Enqueue an outbound message (CRLF added by the codec).
    virtual core::Status<> send(const Message& msg) = 0;

    /// The server's announced hostname, used as the prefix on numerics.
    virtual std::string_view server_name() const noexcept = 0;

    /// Request orderly session shutdown.
    virtual void close() = 0;
};

}  // namespace pvpgn::protocol::irc
