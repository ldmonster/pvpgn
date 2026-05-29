// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file channel_snapshot.hpp
/// `ChannelSnapshot` — a plain value struct for serializing/deserializing
/// Channel aggregate state to/from persistence.

#include <cstdint>
#include <string>
#include <vector>

namespace pvpgn::domain::chat {

struct ChannelSnapshot {
    std::uint64_t               id;
    std::string                 name;
    std::string                 topic;
    bool                        permanent;              // don't delete when empty
    std::uint32_t               max_members;            // 0 = unlimited
    std::string                 client_tag_restriction; // empty = any tag
    std::vector<std::uint64_t>  member_account_ids;
    std::vector<std::uint64_t>  banned_account_ids;
};

}  // namespace pvpgn::domain::chat
