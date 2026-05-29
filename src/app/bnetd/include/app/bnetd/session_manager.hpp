// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file session_manager.hpp
/// Thread-safe registry of all active v3 bnetd sessions.
///
/// `SessionManager` is the composition root's single source of truth for
/// "who is connected right now". It is injected into every FSM factory so
/// that:
///   * The BnetFsm can broadcast ChatEvents to channel members.
///   * The WolFsm can send PRIVMSG to other WOL users.
///   * The admin telnet console can enumerate / kick sessions.
///
/// Design notes
/// ------------
///  * Keyed by `domain::SessionId` (uint64 strong-typedef).
///  * Values are `weak_ptr<ISessionContext>` — the session itself owns the
///    strong reference; the manager never prolongs lifetime.
///  * `broadcast_to_channel` is a best-effort scatter: expired weak_ptrs
///    are silently skipped and removed lazily on the next write.
///  * All public methods are thread-safe (shared_mutex for reads,
///    unique_lock for writes).
///
/// The interface is intentionally narrow. Higher-level routing (e.g.
/// "send to all members of channel X") belongs in `application/chat` or
/// `infra/routing`; this class only provides the raw session map.

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>
#include <string>
#include <unordered_map>

#include "domain/shared/ids.hpp"
#include "protocol/bnet/session_context.hpp"

namespace pvpgn::app::bnetd {

/// Thread-safe registry of active BNet sessions.
///
/// The manager holds `weak_ptr<protocol::bnet::ISessionContext>` so it
/// never artificially extends session lifetime. Callers that need to send
/// data must lock the weak_ptr themselves.
class SessionManager {
public:
    SessionManager()  = default;
    ~SessionManager() = default;

    SessionManager(const SessionManager&)            = delete;
    SessionManager& operator=(const SessionManager&) = delete;
    SessionManager(SessionManager&&)                 = delete;
    SessionManager& operator=(SessionManager&&)      = delete;

    // -----------------------------------------------------------------------
    // Registration
    // -----------------------------------------------------------------------

    /// Register a new session. Overwrites any stale entry for the same id.
    void register_session(
        domain::SessionId                                    session_id,
        std::weak_ptr<protocol::bnet::ISessionContext>       ctx);

    /// Remove a session from the registry. No-op if not found.
    void unregister_session(domain::SessionId session_id);

    // -----------------------------------------------------------------------
    // Lookup
    // -----------------------------------------------------------------------

    /// Resolve a session id to a live context.
    /// Returns `nullopt` if the session is not registered or has expired.
    [[nodiscard]] std::optional<std::shared_ptr<protocol::bnet::ISessionContext>>
    find_session(domain::SessionId session_id) const;

    // -----------------------------------------------------------------------
    // Broadcast
    // -----------------------------------------------------------------------

    /// Invoke `fn` on every live session except `exclude_id`.
    /// Expired weak_ptrs are pruned during the iteration.
    /// `fn` receives a `shared_ptr<ISessionContext>` that is guaranteed
    /// non-null for the duration of the call.
    void for_each_session(
        domain::SessionId                                                    exclude_id,
        const std::function<void(domain::SessionId,
                                 std::shared_ptr<protocol::bnet::ISessionContext>)>& fn);

    // -----------------------------------------------------------------------
    // Metrics
    // -----------------------------------------------------------------------

    /// Number of currently registered (possibly expired) entries.
    /// Use `session_count_live()` for an accurate live count.
    [[nodiscard]] std::size_t session_count() const;

    /// Number of entries whose weak_ptr is still valid.
    [[nodiscard]] std::size_t session_count_live() const;

private:
    struct Entry {
        std::weak_ptr<protocol::bnet::ISessionContext> ctx;
    };

    mutable std::shared_mutex                                    mu_;
    std::unordered_map<std::uint64_t, Entry>                     sessions_;
};

}  // namespace pvpgn::app::bnetd
