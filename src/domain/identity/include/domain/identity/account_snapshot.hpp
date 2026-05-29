// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file account_snapshot.hpp
/// `AccountSnapshot` — a plain value struct for serializing/deserializing
/// Account aggregate state to/from persistence (disk, DB).
///
/// Snapshots have no behavior, only fields. The aggregate's `rehydrate()`
/// factory method reconstructs the aggregate from a snapshot.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/clock.hpp"
#include "domain/shared/ban.hpp"

namespace pvpgn::domain::identity {

struct AccountSnapshot {
    std::uint64_t   id;
    std::string     name;
    std::string     locale_code;           // 4-char (e.g., "enUS")
    std::string     password_hash_hex;     // 40-hex chars (20 bytes)
    bool            locked;
    bool            must_change_password;
    std::vector<std::uint8_t> command_groups;  // 1..8, enabled groups
    std::optional<Ban> active_ban;
    core::SystemTime created_at;
    core::SystemTime updated_at;
};

}  // namespace pvpgn::domain::identity
