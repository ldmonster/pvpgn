// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file report_game_result.hpp
/// REPORT_GAME_RESULT use-case — finalize a game and compute Elo deltas.
///
/// The reporter provides the final game state (winners/losers, disconnects).
/// The use-case:
///   1. Finds the game by ID
///   2. Calls game.finalize() to transition to Finalized state
///   3. Computes Elo delta for each player via LadderCalculator
///   4. Saves updated LadderEntry for each player
///   5. Publishes domain events

#include <memory>
#include <vector>

#include "core/clock.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::ports {
class IGameRepository;
class ILadderRepository;
class IEventBus;
}  // namespace pvpgn::application::ports

namespace pvpgn::application::game {

struct PlayerResult {
    domain::AccountId account_id;
    bool              won;
    bool              disconnected;
};

struct ReportGameResultRequest {
    domain::GameId                   game_id;
    domain::AccountId                reporter_id;
    std::vector<PlayerResult>        results;
    core::SystemTime                 reported_at;
};

enum class ReportGameResultError : std::uint8_t {
    GameNotFound,
    InvalidReporter,
    GameNotInProgress,
    MissingResults,
    PersistenceFailed,
    Internal,
};

class ReportGameResult {
public:
    ReportGameResult(std::shared_ptr<application::ports::IGameRepository> games,
                     std::shared_ptr<application::ports::ILadderRepository> ladder,
                     std::shared_ptr<application::ports::IEventBus> event_bus)
        : games_(games), ladder_(ladder), event_bus_(event_bus) {}

    core::Result<void, ReportGameResultError>
    execute(const ReportGameResultRequest& req);

private:
    std::shared_ptr<application::ports::IGameRepository> games_;
    std::shared_ptr<application::ports::ILadderRepository> ladder_;
    std::shared_ptr<application::ports::IEventBus> event_bus_;
};

}  // namespace pvpgn::application::game
