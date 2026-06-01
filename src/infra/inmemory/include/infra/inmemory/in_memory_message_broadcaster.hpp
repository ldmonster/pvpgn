// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file in_memory_message_broadcaster.hpp
/// In-memory fake for IMessageBroadcaster.
/// Captures sent messages for test inspection.
/// Suitable for tests and development/CI composition roots.

#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "domain/chat/ports.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::infra::inmemory {

class InMemoryMessageBroadcaster final
    : public application::ports::IMessageBroadcaster {
public:
    void
    broadcast_to_channel(domain::ChannelId channel_id,
                         std::string_view  message) override {
        std::unique_lock lock(mutex_);
        messages_.push_back("[channel:" + std::to_string(channel_id.value()) +
                            "] " + std::string{message});
    }

    void
    send_info(domain::ConnectionId connection_id,
              std::string_view     message) override {
        std::unique_lock lock(mutex_);
        messages_.push_back("[info:" + std::to_string(connection_id.value()) +
                            "] " + std::string{message});
    }

    void
    send_error(domain::ConnectionId connection_id,
               std::string_view     message) override {
        std::unique_lock lock(mutex_);
        messages_.push_back("[error:" + std::to_string(connection_id.value()) +
                            "] " + std::string{message});
    }

    /// Return all messages captured so far (test helper).
    [[nodiscard]] std::vector<std::string> last_messages() const {
        std::unique_lock lock(mutex_);
        return messages_;
    }

    /// Clear the captured message log (test helper).
    void clear() {
        std::unique_lock lock(mutex_);
        messages_.clear();
    }

private:
    mutable std::mutex mutex_;
    std::vector<std::string> messages_;
};

}  // namespace pvpgn::infra::inmemory
