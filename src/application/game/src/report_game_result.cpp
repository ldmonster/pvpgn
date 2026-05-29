// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/game/report_game_result.hpp"

#include "application/ports/event_bus.hpp"
#include "application/ports/game_repository.hpp"
#include "application/ports/ladder_repository.hpp"

namespace pvpgn::application::game {

core::Result<void, ReportGameResultError>
ReportGameResult::execute(const ReportGameResultRequest& req) {
    // 1. Validate request
    if (req.results.empty()) {
        return core::fail(ReportGameResultError::MissingResults);
    }

    // 2. Find the game
    auto game_result = games_->find_by_id(req.game_id.value());
    if (!game_result) {
        return core::fail(ReportGameResultError::GameNotFound);
    }

    auto game_ptr = game_result.value();
    auto& game = *game_ptr;

    // 3. Verify reporter is in the game
    if (!game.contains(req.reporter_id)) {
        return core::fail(ReportGameResultError::InvalidReporter);
    }

    // 4. Begin reporting phase
    if (!game.begin_report()) {
        return core::fail(ReportGameResultError::GameNotInProgress);
    }

    // 5. Convert to domain PlayerResults
    std::vector<domain::PlayerResult> player_results;
    for (const auto& res : req.results) {
        domain::MatchOutcome outcome = domain::MatchOutcome::Loss;
        if (res.disconnected) {
            outcome = domain::MatchOutcome::Disconnect;
        } else if (res.won) {
            outcome = domain::MatchOutcome::Win;
        }
        player_results.push_back({
            .account = res.account_id,
            .outcome = outcome,
        });
    }

    // 6. Finalize the game
    auto finalize_outcome = game.finalize(player_results, req.reported_at);
    if (finalize_outcome == domain::gameplay::Game::EndOutcome::NotInProgress) {
        return core::fail(ReportGameResultError::GameNotInProgress);
    }

    // 7. Save the game
    auto save_result = games_->save(game);
    if (!save_result) {
        return core::fail(ReportGameResultError::PersistenceFailed);
    }

    // 8. Drain events and publish
    auto events = game.drain_events();
    for (const auto& event : events) {
        event_bus_->publish(event);
    }

    return core::Result<void, ReportGameResultError>{};
}

}  // namespace pvpgn::application::game
