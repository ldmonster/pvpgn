// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file game.hpp
/// `gameplay::Game` aggregate — a single hosted match.
///
/// Owns a deterministic FSM:
///
///     Open ──host.start()──▶ InProgress ──any.report()──▶ Reporting
///                                                           │
///                                              ┌────────────┘
///                                              ▼
///                                          Finalized
///
/// The aggregate keeps no clock state of its own — wall-clock is
/// always provided by the caller (`core::SystemTime`).

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "core/clock.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/events.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/match_report.hpp"

namespace pvpgn::domain::gameplay {

enum class GameState : std::uint8_t {
    Open,
    InProgress,
    Reporting,
    Finalized,
};

struct GameDescriptor {
    std::string   name;
    std::string   map;
    std::uint8_t  max_players = 8;
    /// Client-supplied game type (BNet "bngtype" wire value from SID_STARTADVEX3),
    /// echoed back in each SID_GETADVLISTEX record. 0 = unspecified/all.
    std::uint16_t gametype = 0;
};

class Game {
public:
    enum class JoinOutcome  : std::uint8_t { Joined, AlreadyIn, Full, Closed };
    enum class StartOutcome : std::uint8_t { Started, NotHost, WrongState };
    enum class EndOutcome   : std::uint8_t { Finalized, NotInProgress };

    static core::Result<Game> host(GameId id, AccountId host, ClientTag client,
                                   GameDescriptor desc) {
        if (desc.name.empty()) {
            return core::fail(core::Error{
                core::StatusCode::InvalidArgument, "game name empty"});
        }
        if (desc.max_players == 0 || desc.max_players > 16) {
            return core::fail(core::Error{
                core::StatusCode::InvalidArgument, "max_players must be 1..16"});
        }
        Game g{id, host, client, std::move(desc)};
        g.players_.push_back(host);
        g.events_.push_back(events::GameCreated{id, host, client});
        g.events_.push_back(events::GamePlayerJoined{id, host});
        return g;
    }

    /// Reconstruct a Game from persisted state without emitting events or
    /// re-running `host()`'s validation. For repositories only.
    static Game rehydrate(GameId id, AccountId host, ClientTag client,
                          GameDescriptor desc, GameState state,
                          std::vector<AccountId> players) {
        Game g{id, host, client, std::move(desc)};
        g.state_   = state;
        g.players_ = std::move(players);
        return g;
    }

    // --- Queries --------------------------------------------------------

    GameId                          id()           const noexcept { return id_; }
    AccountId                       host()         const noexcept { return host_; }
    ClientTag                       client()       const noexcept { return client_; }
    const GameDescriptor&           descriptor()   const noexcept { return desc_; }
    GameState                       state()        const noexcept { return state_; }
    const std::vector<AccountId>&   players()      const noexcept { return players_; }
    std::size_t                     player_count() const noexcept { return players_.size(); }
    bool                            is_full()      const noexcept {
        return players_.size() >= desc_.max_players;
    }
    bool contains(AccountId a) const noexcept {
        return std::find(players_.begin(), players_.end(), a) != players_.end();
    }

    // --- Commands -------------------------------------------------------

    JoinOutcome join(AccountId who) {
        if (state_ != GameState::Open)  return JoinOutcome::Closed;
        if (contains(who))              return JoinOutcome::AlreadyIn;
        if (is_full())                  return JoinOutcome::Full;
        players_.push_back(who);
        events_.push_back(events::GamePlayerJoined{id_, who});
        return JoinOutcome::Joined;
    }

    bool leave(AccountId who) {
        auto it = std::find(players_.begin(), players_.end(), who);
        if (it == players_.end()) return false;
        players_.erase(it);
        events_.push_back(events::GamePlayerLeft{id_, who});
        // Invariant: the host is always a current player. If the host left and
        // players remain, migrate the host to the next remaining player so the
        // aggregate never reports a departed account as its host. (When the
        // last player leaves, the game is empty and the caller deletes it.)
        if (who == host_ && !players_.empty()) {
            host_ = players_.front();
        }
        return true;
    }

    StartOutcome start(AccountId by, core::SystemTime now) {
        if (state_ != GameState::Open)  return StartOutcome::WrongState;
        if (by != host_)                return StartOutcome::NotHost;
        state_ = GameState::InProgress;
        events_.push_back(events::GameStarted{id_, now});
        return StartOutcome::Started;
    }

    /// Transition InProgress → Reporting. Subsequent calls in
    /// `Reporting` are accepted but state stays `Reporting`.
    bool begin_report() {
        if (state_ == GameState::InProgress) {
            state_ = GameState::Reporting;
            return true;
        }
        return state_ == GameState::Reporting;
    }

    EndOutcome finalize(std::vector<PlayerResult> results, core::SystemTime now) {
        if (state_ != GameState::InProgress && state_ != GameState::Reporting) {
            return EndOutcome::NotInProgress;
        }
        state_ = GameState::Finalized;
        MatchReport report{id_, client_, std::move(results), now};
        events_.push_back(events::GameEnded{id_, std::move(report)});
        return EndOutcome::Finalized;
    }

    std::vector<events::DomainEvent> drain_events() {
        return std::exchange(events_, {});
    }

private:
    Game(GameId id, AccountId host, ClientTag client, GameDescriptor desc)
        : id_(id), host_(host), client_(client), desc_(std::move(desc)) {}

    GameId                              id_;
    AccountId                           host_;
    ClientTag                           client_;
    GameDescriptor                      desc_;
    GameState                           state_ = GameState::Open;
    std::vector<AccountId>              players_;
    std::vector<events::DomainEvent>    events_;
};

}  // namespace pvpgn::domain::gameplay
