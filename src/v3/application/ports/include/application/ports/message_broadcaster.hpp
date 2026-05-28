// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file message_broadcaster.hpp
/// Port: outbound message fan-out to channels and individual connections.

#include <string_view>

#include "domain/shared/ids.hpp"

namespace pvpgn::application::ports {

/// Port: message broadcaster interface for hexagonal architecture.
/// Implementations fan out to the active connection registry held by the
/// network layer (infra/).
class IMessageBroadcaster {
public:
    virtual ~IMessageBroadcaster() = default;

    /// Broadcast a text message to every connection currently in `channel_id`.
    virtual void
    broadcast_to_channel(domain::ChannelId   channel_id,
                         std::string_view    message) = 0;

    /// Send a server info message to a single connection.
    virtual void
    send_info(domain::ConnectionId connection_id,
              std::string_view     message) = 0;

    /// Send an error message to a single connection.
    virtual void
    send_error(domain::ConnectionId connection_id,
               std::string_view     message) = 0;
};

}  // namespace pvpgn::application::ports
