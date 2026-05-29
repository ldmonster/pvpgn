// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file game_snapshot.hpp
/// `GameSnapshot` — a plain value struct for serializing/deserializing
/// Game aggregate state to/from persistence.

#include <cstdint>
#include <string>
#include <vector>

#include "core/clock.hpp"

namespace pvpgn::domain::gameplay {

struct GameSnapshot {
    std::uint64_t               id;
    std::string                 name;
    std::uint64_t               host_account_id;
    std::string                 game_type;          // "melee", "ffa", "1v1", etc.
    std::uint32_t               max_players;
    std::string                 map_name;
    std::string                 server_ip;
    std::uint16_t               server_port;
    std::string                 state;              // "open", "in_progress", "reporting", "finalized"
    std::vector<std::uint64_t>  player_account_ids;
    core::SystemTime            created_at;
};

}  // namespace pvpgn::domain::gameplay
