// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file clan_snapshot.hpp
/// `ClanSnapshot` — a plain value struct for serializing/deserializing
/// Clan aggregate state to/from persistence.

#include <cstdint>
#include <string>
#include <vector>

#include "core/clock.hpp"

namespace pvpgn::domain::social {

struct ClanMemberSnapshot {
    std::uint64_t   account_id;
    std::string     rank;               // "chieftain", "shaman", "grunt", "peon"
    core::SystemTime joined_at;
};

struct ClanSnapshot {
    std::uint64_t                       id;
    std::string                         tag;                // 2-4 chars
    std::string                         name;
    std::uint64_t                       founder_id;
    std::vector<ClanMemberSnapshot>     members;
    core::SystemTime                    created_at;
};

}  // namespace pvpgn::domain::social
