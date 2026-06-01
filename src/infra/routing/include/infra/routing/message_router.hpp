// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file message_router.hpp
/// Thread-safe implementation of IMessageRouter.
/// Maintains a SessionId → IConnectionEgress mapping and forwards
/// encoded bytes through registered egress channels.

#include <cstddef>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <unordered_map>
#include <vector>

#include "domain/connection/ports.hpp"
#include "domain/connection/ports.hpp"
#include "domain/identity/ports.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::infra::routing {

/// Concrete implementation of IMessageRouter.
/// - Holds weak_ptr to ISessionRegistry for account lookup
/// - Maintains SessionId → weak_ptr<IConnectionEgress> map
/// - Thread-safe with shared_mutex
class MessageRouterImpl final : public application::ports::IMessageRouter {
public:
    explicit MessageRouterImpl(
        std::weak_ptr<application::ports::ISessionRegistry> registry)
        : registry_(registry) {}

    /// Register a session's egress channel.
    /// Called when a new session is created.
    void register_session(domain::SessionId session_id,
                          std::shared_ptr<application::ports::IConnectionEgress>
                              egress) {
        std::unique_lock lock(mu_);
        sessions_[session_id.value()] = egress;
    }

    /// Unregister a session (remove it from the routing map).
    /// Called when a session closes.
    void unregister_session(domain::SessionId session_id) {
        std::unique_lock lock(mu_);
        sessions_.erase(session_id.value());
    }

    // IMessageRouter implementation

    core::Result<void, core::Error> send(domain::SessionId session_id,
                                         std::span<const std::byte> bytes) override {
        std::shared_lock lock(mu_);
        auto it = sessions_.find(session_id.value());
        if (it == sessions_.end()) {
            return core::fail(core::Error{core::StatusCode::NotFound,
                                          "session not found"});
        }
        auto egress_ptr = it->second.lock();
        if (!egress_ptr) {
            return core::fail(core::Error{core::StatusCode::Internal,
                                          "egress reference expired"});
        }
        lock.unlock();

        // Convert span to vector for IConnectionEgress::send()
        std::vector<std::byte> data(bytes.begin(), bytes.end());
        egress_ptr->send(std::move(data));
        return core::ok();
    }

    core::Result<void, core::Error> broadcast(
        std::span<const domain::SessionId> sessions,
        std::span<const std::byte> bytes) override {
        std::shared_lock lock(mu_);

        // Convert span to vector once (shared across all sends)
        std::vector<std::byte> data(bytes.begin(), bytes.end());

        for (const auto& session_id : sessions) {
            auto it = sessions_.find(session_id.value());
            if (it == sessions_.end()) continue;

            auto egress_ptr = it->second.lock();
            if (!egress_ptr) continue;

            // Unlock briefly to send, then reacquire
            lock.unlock();
            egress_ptr->send(data);  // Copies the vector
            lock.lock();
        }

        return core::ok();
    }

    core::Result<void, core::Error> send_to_account(
        domain::AccountId account_id, std::span<const std::byte> bytes) override {
        // Look up registry
        auto registry_ptr = registry_.lock();
        if (!registry_ptr) {
            return core::fail(core::Error{core::StatusCode::Internal,
                                          "session registry not available"});
        }

        // Lookup session for account
        auto session_opt = registry_ptr->session_for(account_id);
        if (!session_opt) {
            return core::fail(core::Error{core::StatusCode::NotFound,
                                          "no session for account"});
        }

        // Forward to send()
        return send(session_opt.value(), bytes);
    }

private:
    std::weak_ptr<application::ports::ISessionRegistry> registry_;
    mutable std::shared_mutex mu_;
    std::unordered_map<std::uint64_t,
                       std::weak_ptr<application::ports::IConnectionEgress>>
        sessions_;
};

}  // namespace pvpgn::infra::routing
