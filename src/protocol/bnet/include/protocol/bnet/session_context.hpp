// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file session_context.hpp
/// Abstract dependency exposed by the BNet FSM. Concrete sessions
/// (`infra/net` fibers, replay harness, fuzz target) implement this.
/// The FSM is otherwise pure — no I/O, no clock, no logging.

#include "core/result.hpp"
#include "protocol/bnet/messages.hpp"

namespace pvpgn::protocol::bnet {

class ISessionContext {
public:
    virtual ~ISessionContext() = default;

    /// Enqueue an outbound message for transmission.
    virtual core::Status<> send(const ServerMessage& msg) = 0;

    /// Request orderly session shutdown.
    virtual void close() = 0;
};

}  // namespace pvpgn::protocol::bnet
