// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/chat/list_channels.hpp"

#include "application/ports/channel_repository.hpp"
#include "domain/chat/channel.hpp"

namespace pvpgn::application::chat {

core::Result<std::vector<ChannelInfo>, ListChannelsError>
ListChannels::execute(const ListChannelsRequest& req) const {
    // 1. Validate max_results
    if (req.max_results == 0) {
        return core::fail(ListChannelsError::InvalidMaxResults);
    }

    std::vector<ChannelInfo> result;

    // 2. Iterate channels via forEach
    channels_->forEach([&](domain::chat::Channel& ch) {
        // 3. Filter by client tag if provided
        if (req.filter_by_tag) {
            if (!(ch.policy().client == *req.filter_by_tag)) {
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
        return result.size() < req.max_results;
    });

    return result;
}

}  // namespace pvpgn::application::chat
