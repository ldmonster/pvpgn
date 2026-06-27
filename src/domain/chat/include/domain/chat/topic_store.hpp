// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file topic_store.hpp
/// ITopicStore — a channel-NAME-keyed, persistent topic store whose entries live
/// independently of any Channel object's lifetime.
///
/// Mirrors the original server's `class_topiclist` (topic.cpp): IRC/WOL channel
/// topics are kept in a name-keyed map that is NOT destroyed together with the
/// Channel. When the last member of a non-permanent channel leaves and the
/// Channel object is torn down, the topic survives here, so a fresh re-joiner of
/// the same channel name is shown the previously-set topic (RPL_TOPIC 332).
///
/// Without this, v3 over-cleans: storing the topic on the Channel domain object
/// discards it on the destroy-on-empty path, and the re-joiner sees an empty
/// topic — a divergence from the oracle.
///
/// Entries are keyed by the channel's canonical (case-folded) name.

#include <string>
#include <string_view>

namespace pvpgn::domain::chat {

class ITopicStore {
public:
    virtual ~ITopicStore() = default;

    /// Store (insert or replace) the topic for `channel_name`.
    virtual void set(std::string_view channel_name, std::string topic) = 0;

    /// Return the stored topic for `channel_name`, or an empty string when none
    /// has been set.
    [[nodiscard]] virtual std::string
    get(std::string_view channel_name) const = 0;

protected:
    ITopicStore() = default;
};

}  // namespace pvpgn::domain::chat
