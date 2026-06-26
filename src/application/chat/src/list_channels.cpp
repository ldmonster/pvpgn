// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/chat/list_channels.hpp"

#include <algorithm>

#include "domain/chat/ports.hpp"
#include "domain/chat/channel.hpp"

namespace pvpgn::application::chat {

core::Result<std::vector<ChannelInfo>, ListChannelsError>
ListChannels::execute(const ListChannelsRequest& req) const {
    // 1. Validate max_results (0 is invalid when not filtering by tag)
    bool has_filter = req.filter_by_tag.has_value() &&
                      req.filter_by_tag.value().bytes() != domain::ClientTag{}.bytes();
    if (req.max_results == 0 && !has_filter) {
        return core::fail(ListChannelsError::InvalidMaxResults);
    }

    std::vector<ChannelInfo> result;
    
    // Use a large number if max_results is 0 (unlimited when filtering)
    std::uint32_t limit = (req.max_results == 0) ? UINT32_MAX : req.max_results;

    // 2. Iterate channels via forEach
    channels_->forEach([&](const domain::chat::Channel& ch) {
        // "the Void" sink channel is never advertised in any listing, matching
        // the original server (channel_flags_thevoid is excluded from
        // SID_CHANNELLIST, the /channels command, and the IRC/WOL LIST).
        if (ch.policy().flags.has(domain::chat::ChannelFlag::TheVoid)) {
            return true;  // continue — skip
        }

        // 3. Filter by client tag if provided
        if (req.filter_by_tag.has_value() &&
            req.filter_by_tag.value().bytes() != domain::ClientTag{}.bytes()) {
            if (ch.policy().client.bytes() != req.filter_by_tag.value().bytes()) {
                return true;  // continue
            }
        }

        // 4. Collect channel info
        result.push_back(ChannelInfo{
            .id = ch.id(),
            .name = ch.name(),
            .member_count = ch.member_count(),
            .client_tag_restriction = ch.policy().client,
        });

        // 5. Respect max_results limit
        return result.size() < limit;
    });

    // 6. Sort by channel ID for consistent ordering
    std::sort(result.begin(), result.end(),
              [](const ChannelInfo& a, const ChannelInfo& b) {
                  return a.id.value() < b.id.value();
              });

    return result;
}

}  // namespace pvpgn::application::chat
