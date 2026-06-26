// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file wol_session_context.hpp
/// Abstract I/O dependency for the WOL (Westwood Online) chat FSM.
///
/// WOL is an IRC-like text protocol used by Command & Conquer, Red Alert,
/// and other Westwood games. This interface separates the FSM from the
/// transport layer (Asio TcpSession, test fakes, etc.).

#include <span>
#include <string_view>
#include <cstddef>

#include "core/result.hpp"

namespace pvpgn::protocol::wol {

/// Abstract session context for the WOL chat FSM.
/// Concrete implementations live in src/app/bnetd (Asio session factories) and
/// tests/unit (FakeWolContext).
class IWolSessionContext {
public:
    virtual ~IWolSessionContext() = default;

    /// Send a raw text line to the client.
    /// The implementation must append \r\n if not already present.
    virtual core::Status<> send_line(std::string_view line) = 0;

    /// Send raw bytes (for bulk data).
    virtual core::Status<> send_bytes(std::span<const std::byte> bytes) = 0;

    /// Request orderly session shutdown.
    virtual void close() = 0;

    /// The server's hostname, used as prefix on numeric replies.
    virtual std::string_view server_name() const noexcept = 0;
};

}  // namespace pvpgn::protocol::wol
