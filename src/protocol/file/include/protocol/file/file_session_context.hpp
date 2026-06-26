// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file file_session_context.hpp
/// Abstract I/O dependency for the BNFTP FSM.
///
/// Separates the FSM from the transport layer (Asio TcpSession,
/// test fakes, etc.). The FSM only calls `send_bytes()` and `close()`.

#include <span>
#include <cstddef>
#include <vector>

#include "core/result.hpp"

namespace pvpgn::protocol::file {

/// Abstract session context for the BNFTP file-transfer FSM.
/// Concrete implementations live in src/app/bnetd (Asio session factories) and
/// tests/unit (FakeFileContext).
class IFileSessionContext {
public:
    virtual ~IFileSessionContext() = default;

    /// Enqueue raw bytes for transmission. The span is valid only
    /// during the call; implementations must copy if needed.
    virtual core::Status<> send_bytes(std::span<const std::byte> bytes) = 0;

    /// Request orderly session shutdown.
    virtual void close() = 0;
};

}  // namespace pvpgn::protocol::file
