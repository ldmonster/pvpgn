// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file telnet_session_context.hpp
/// Abstract dependency exposed by the Telnet admin console FSM.
///
/// Mirrors ``protocol::bnet::ISessionContext`` but exposes a plain
/// byte/text ``send`` surface, since the telnet wire format is line-
/// oriented ASCII rather than ``ServerMessage`` variants.

#include <string_view>

#include "core/bytes.hpp"
#include "core/result.hpp"

namespace pvpgn::protocol::telnet {

class ITelnetSessionContext {
public:
    virtual ~ITelnetSessionContext() = default;

    /// Enqueue raw bytes for transmission.
    virtual core::Status<> send(core::ByteView bytes) = 0;

    /// Convenience wrapper -- forwards a textual line plus CRLF.
    virtual core::Status<> send_line(std::string_view text) = 0;

    /// Request orderly session shutdown.
    virtual void close() = 0;
};

}  // namespace pvpgn::protocol::telnet
