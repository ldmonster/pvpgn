// SPDX-License-Identifier: GPL-2.0-or-later

/// @file session_manager.cpp
/// Implementation of `SessionManager`.

#include "app/bnetd/session_manager.hpp"

#include <algorithm>
#include <vector>

namespace pvpgn::app::bnetd {

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void SessionManager::register_session(
    domain::SessionId                              session_id,
    std::weak_ptr<protocol::bnet::ISessionContext> ctx) {
    std::unique_lock lock(mu_);
    sessions_[session_id.value()] = Entry{std::move(ctx)};
}

void SessionManager::unregister_session(domain::SessionId session_id) {
    std::unique_lock lock(mu_);
    sessions_.erase(session_id.value());
}

// ---------------------------------------------------------------------------
// Lookup
// ---------------------------------------------------------------------------

std::optional<std::shared_ptr<protocol::bnet::ISessionContext>>
SessionManager::find_session(domain::SessionId session_id) const {
    std::shared_lock lock(mu_);
    auto it = sessions_.find(session_id.value());
    if (it == sessions_.end()) return std::nullopt;
    auto sp = it->second.ctx.lock();
    if (!sp) return std::nullopt;
    return sp;
}

// ---------------------------------------------------------------------------
// Broadcast
// ---------------------------------------------------------------------------

void SessionManager::for_each_session(
    domain::SessionId exclude_id,
    const std::function<void(domain::SessionId,
                             std::shared_ptr<protocol::bnet::ISessionContext>)>& fn) {
    // Collect live sessions under the read lock, then invoke callbacks
    // outside the lock to avoid deadlocks if `fn` calls back into the manager.
    struct LiveEntry {
        domain::SessionId                              id;
        std::shared_ptr<protocol::bnet::ISessionContext> ctx;
    };
    std::vector<LiveEntry> live;

    {
        std::shared_lock lock(mu_);
        live.reserve(sessions_.size());
        for (auto& [raw_id, entry] : sessions_) {
            if (raw_id == exclude_id.value()) continue;
            auto sp = entry.ctx.lock();
            if (sp) live.emplace_back(LiveEntry{domain::SessionId{raw_id}, std::move(sp)});
        }
    }

    for (auto& e : live) {
        fn(e.id, std::move(e.ctx));
    }

    // Lazily prune expired entries (best-effort; no guarantee of ordering).
    {
        std::unique_lock lock(mu_);
        for (auto it = sessions_.begin(); it != sessions_.end(); ) {
            if (it->second.ctx.expired()) {
                it = sessions_.erase(it);
            } else {
                ++it;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Metrics
// ---------------------------------------------------------------------------

std::size_t SessionManager::session_count() const {
    std::shared_lock lock(mu_);
    return sessions_.size();
}

std::size_t SessionManager::session_count_live() const {
    std::shared_lock lock(mu_);
    std::size_t count = 0;
    for (auto& [id, entry] : sessions_) {
        if (!entry.ctx.expired()) ++count;
    }
    return count;
}

}  // namespace pvpgn::app::bnetd
