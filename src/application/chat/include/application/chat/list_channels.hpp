// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file list_channels.hpp
/// LIST_CHANNELS use-case — enumerate available channels with filtering.
///
/// Returns a paginated list of channels with optional filtering by client tag.

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "domain/chat/ports.hpp"

namespace pvpgn::application::chat {

/// Public information about a channel.
struct ChannelInfo {
    domain::ChannelId     id;
    std::string           name;
    std::size_t           member_count;
    domain::ClientTag     client_tag_restriction;  // empty = open to all
};

/// Request parameters for listing channels.
struct ListChannelsRequest {
    std::optional<domain::ClientTag> filter_by_tag;  // nullopt = all channels
    std::uint32_t                    max_results{50};
};

/// Errors that can occur during channel listing.
enum class ListChannelsError : std::uint8_t {
    InvalidMaxResults,
};

class ListChannels {
public:
    explicit ListChannels(
        std::shared_ptr<domain::chat::IChannelRepository> channels)
        : channels_(channels) {}

    /// Execute: list channels with optional filtering.
    core::Result<std::vector<ChannelInfo>, ListChannelsError>
    execute(const ListChannelsRequest& req) const;

private:
    std::shared_ptr<domain::chat::IChannelRepository> channels_;
};

}  // namespace pvpgn::application::chat
