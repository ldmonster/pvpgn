// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file connection_handler.hpp
/// Application-layer ports for the egress side of a network connection
/// (`IConnectionEgress`) used by protocol FSMs to send encoded bytes
/// back to the client.

#include <cstddef>
#include <utility>
#include <vector>

namespace pvpgn::application::ports {

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

} // namespace pvpgn::application::ports
