// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file message_broadcaster.hpp
/// Port for fanning out server-originated messages to channels /
/// individual connections.

#include <string_view>

#include "domain/shared/ids.hpp"

namespace pvpgn::application::ports {

class IMessageBroadcaster {
public:
    virtual ~IMessageBroadcaster() = default;

    /// Broadcast `message` to every member of `channel_id`.
    virtual void
        broadcast_to_channel(domain::ChannelId channel_id,
                             std::string_view  message) = 0;

    /// Send an INFO-class message to a single connection.
    virtual void
        send_info(domain::ConnectionId connection_id,
                  std::string_view     message) = 0;

    /// Send an ERROR-class message to a single connection.
    virtual void
        send_error(domain::ConnectionId connection_id,
                   std::string_view     message) = 0;
};

} // namespace pvpgn::application::ports
