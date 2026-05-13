// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file match_report.hpp
/// `MatchReport` — the canonical result-of-a-game value object.
/// Emitted by the `gameplay::Game` aggregate, consumed by the
/// `ladder::LadderCalculator` domain service.

#include <cstdint>
#include <vector>

#include "core/clock.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::domain {

enum class MatchOutcome : std::uint8_t {
    Win,
    Loss,
    Draw,
    Disconnect,
};

struct PlayerResult {
    AccountId    account;
    MatchOutcome outcome;
};

struct MatchReport {
    GameId                    game;
    ClientTag                 client;
    std::vector<PlayerResult> results;
    core::SystemTime          finished_at;
};

}  // namespace pvpgn::domain
