// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file d2gs_registry.hpp
/// Run-loop-scoped, thread-safe registry shared by all D2CS sessions to route
/// the game-lobby flow between a client session and a D2GS server link.
///
/// The original d2cs forwards a client CREATEGAMEREQ to a chosen game server
/// (d2gslist_choose_server), waits for the D2GS's CREATEGAMEREPLY, then answers
/// the client. v3 sessions are otherwise isolated (one D2CSTcpSession per
/// connection); this registry is the shared rendezvous:
///   - D2GS links register themselves once authenticated + SETGSINFO'd.
///   - A client create-game request picks a D2GS, records a pending entry keyed
///     by a correlation id, and forwards the request with that id as the d2gs
///     frame seqno.
///   - The D2GS echoes the id in its reply; the d2gs-link session looks the
///     pending entry back up and the client is answered.
///
/// All collections are mutex-guarded; the actual socket writes happen via
/// TcpSession::send (which posts to each session's own strand), so cross-thread
/// calls are safe.

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

namespace pvpgn::app::d2cs {

class D2CSTcpSession;  // sessions call each other; registry only holds weak refs

class D2gsRegistry {
public:
    /// A client request awaiting a D2GS reply.
    struct Pending {
        std::weak_ptr<D2CSTcpSession> client;
        std::uint16_t                 client_seqno = 0;
        std::string                   game_name;
        std::uint32_t                 gameflag = 0;  // for create -> game store
        std::string                   game_desc;
    };

    /// Register an authenticated, SETGSINFO'd D2GS link as choosable.
    void add_d2gs(std::weak_ptr<D2CSTcpSession> gs) {
        std::lock_guard<std::mutex> lk(mu_);
        d2gs_.push_back(std::move(gs));
    }

    /// Drop a D2GS link (called on its disconnect).
    void remove_d2gs(const D2CSTcpSession* gs) {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto it = d2gs_.begin(); it != d2gs_.end();) {
            auto sp = it->lock();
            if (!sp || sp.get() == gs) it = d2gs_.erase(it);
            else ++it;
        }
    }

    /// Pick an active D2GS (round-robin over live links). Null if none.
    std::shared_ptr<D2CSTcpSession> choose_d2gs() {
        std::lock_guard<std::mutex> lk(mu_);
        const std::size_t n = d2gs_.size();
        for (std::size_t i = 0; i < n; ++i) {
            auto& w = d2gs_[(rr_ + i) % n];
            if (auto sp = w.lock()) {
                rr_ = (rr_ + i + 1) % (n ? n : 1);
                return sp;
            }
        }
        return nullptr;
    }

    /// Record a pending client request; returns its correlation id.
    std::uint32_t add_pending(std::weak_ptr<D2CSTcpSession> client,
                              std::uint16_t client_seqno,
                              std::string game_name,
                              std::uint32_t gameflag = 0,
                              std::string game_desc = {}) {
        std::lock_guard<std::mutex> lk(mu_);
        const std::uint32_t corr = next_corr_++;
        pending_.emplace(corr, Pending{std::move(client), client_seqno,
                                       std::move(game_name), gameflag,
                                       std::move(game_desc)});
        return corr;
    }

    /// Take (remove + return) a pending request by correlation id.
    std::optional<Pending> take_pending(std::uint32_t corr) {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = pending_.find(corr);
        if (it == pending_.end()) return std::nullopt;
        Pending p = std::move(it->second);
        pending_.erase(it);
        return p;
    }

    /// A created game, mapping its name to the D2GS hosting it.
    struct GameRec {
        std::uint32_t                 gameid = 0;  // the D2GS's internal id
        std::weak_ptr<D2CSTcpSession> gs;
        std::uint32_t                 game_number = 0;  // d2cs id (1-based, GAMELIST token)
        std::uint32_t                 gameflag = 0;
        std::string                   desc;
        std::uint32_t                 currchar = 0;  // players in the game
    };

    /// A game's listing data (snapshot for GAMELISTREPLY).
    struct GameListEntry {
        std::string   name;
        std::uint32_t game_number = 0;
        std::uint32_t currchar = 0;
        std::uint32_t gameflag = 0;
        std::string   desc;
    };

    /// Record a created game so a later JOINGAMEREQ / GAMELISTREQ can find it.
    /// Assigns the d2cs game number (1-based, the GAMELIST token).
    void add_game(const std::string& name, std::uint32_t gameid,
                  std::weak_ptr<D2CSTcpSession> gs, std::uint32_t gameflag = 0,
                  std::string desc = {}) {
        std::lock_guard<std::mutex> lk(mu_);
        games_[name] = GameRec{gameid, std::move(gs), next_game_number_++,
                               gameflag, std::move(desc), 0};
    }

    /// Look up a game by name; prunes + misses if its host has gone away.
    std::optional<GameRec> find_game(const std::string& name) {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = games_.find(name);
        if (it == games_.end()) return std::nullopt;
        if (it->second.gs.expired()) { games_.erase(it); return std::nullopt; }
        return it->second;
    }

    /// Adjust a game's player count (UPDATEGAMEINFO ENTER/LEAVE). The game is
    /// identified by its host D2GS link + the D2GS's internal game id.
    void adjust_currchar(const D2CSTcpSession* host, std::uint32_t gameid,
                         int delta) {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto& [name, rec] : games_) {
            if (rec.gameid != gameid) continue;
            auto sp = rec.gs.lock();
            if (!sp || sp.get() != host) continue;
            if (delta < 0 && rec.currchar < static_cast<std::uint32_t>(-delta))
                rec.currchar = 0;
            else
                rec.currchar = static_cast<std::uint32_t>(
                    static_cast<int>(rec.currchar) + delta);
            return;
        }
    }

    /// Snapshot of games with at least one player (currchar>0) for GAMELISTREPLY.
    /// Prunes games whose host has disconnected.
    std::vector<GameListEntry> list_active_games() {
        std::lock_guard<std::mutex> lk(mu_);
        std::vector<GameListEntry> out;
        for (auto it = games_.begin(); it != games_.end();) {
            if (it->second.gs.expired()) { it = games_.erase(it); continue; }
            if (it->second.currchar > 0) {
                out.push_back(GameListEntry{it->first, it->second.game_number,
                                            it->second.currchar,
                                            it->second.gameflag, it->second.desc});
            }
            ++it;
        }
        return out;
    }

private:
    std::mutex mu_;
    std::vector<std::weak_ptr<D2CSTcpSession>> d2gs_;
    std::unordered_map<std::uint32_t, Pending> pending_;
    std::unordered_map<std::string, GameRec> games_;
    std::uint32_t next_corr_        = 1;
    std::uint32_t next_game_number_ = 1;  // 1-based d2cs game number (GAMELIST token)
    std::size_t   rr_               = 0;
};

}  // namespace pvpgn::app::d2cs
