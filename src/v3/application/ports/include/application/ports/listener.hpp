// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file listener.hpp
/// Abstract TCP listener port.
///
/// Implementations live in `infra/net/`.
/// Null/fake implementations for tests live in `infra/inmemory/`.

#include <cstdint>
#include <functional>
#include <string>

#include "core/result.hpp"

namespace pvpgn::application::ports {

/// Callback type: called when a new connection is accepted.
/// Parameters: remote IP string, remote port.
using AcceptCallback =
    std::function<void(std::string remote_ip, std::uint16_t remote_port)>;

class IListener {
public:
    virtual ~IListener() = default;

    /// Start listening on the given port; calls callback for each accepted connection.
    virtual core::Status<void> listen(std::uint16_t port,
                                      AcceptCallback on_accept) = 0;

    /// Stop accepting new connections.
    virtual void close() noexcept = 0;

    /// Returns the port currently listening on (0 if not listening).
    virtual std::uint16_t local_port() const noexcept = 0;
};

}  // namespace pvpgn::application::ports
