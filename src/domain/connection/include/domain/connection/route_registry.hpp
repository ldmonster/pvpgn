// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file route_registry.hpp
/// WAR3 route-connection pairing registry.
///
/// WAR3 clients open a second TCP connection for game routing.  Both the
/// primary connection and the route connection carry the same 4-byte token
/// (extracted from SID_WARCRAFTGENERAL 0x44).  RouteRegistry maps that token
/// to the owning ConnectionFsm so the two connections can be paired.
///
/// ## Lifecycle
///
///   1. Primary connection receives SID_WARCRAFTGENERAL → calls
///      `ConnectionFsm::set_war3_route_token(token)`.
///   2. Caller registers the primary FSM: `registry.register_primary(token, &fsm)`.
///   3. Route connection arrives and presents the same token.
///   4. Caller looks up: `registry.find_primary(token)` → non-null pointer.
///   5. Pairing complete; caller removes the entry: `registry.unregister(token)`.
///
/// ## Thread safety
///
///   Not thread-safe.  All calls must be made from the same I/O thread (or
///   under an external lock).  The registry does not own the ConnectionFsm
///   objects — callers are responsible for ensuring the FSM outlives any
///   registry entry that references it.

#include <cstdint>
#include <unordered_map>

namespace pvpgn::application::connection {
class ConnectionFsm;
}  // namespace pvpgn::application::connection

namespace pvpgn::domain::connection {

using pvpgn::application::connection::ConnectionFsm;

/// Simple token → ConnectionFsm* map for WAR3 route-connection pairing.
class RouteRegistry {
public:
    RouteRegistry()  = default;
    ~RouteRegistry() = default;

    // Non-copyable (contains raw pointers that must not be duplicated).
    RouteRegistry(const RouteRegistry&)            = delete;
    RouteRegistry& operator=(const RouteRegistry&) = delete;

    // Movable.
    RouteRegistry(RouteRegistry&&)            = default;
    RouteRegistry& operator=(RouteRegistry&&) = default;

    // -----------------------------------------------------------------------
    // Mutators
    // -----------------------------------------------------------------------

    /// Register a primary ConnectionFsm under the given route token.
    /// If a previous entry exists for the same token it is overwritten.
    /// @param token  4-byte route token from SID_WARCRAFTGENERAL.
    /// @param fsm    Non-owning pointer to the primary ConnectionFsm.
    ///               Must not be null; must outlive the registry entry.
    void register_primary(std::uint32_t token, ConnectionFsm* fsm) {
        map_[token] = fsm;
    }

    /// Remove the entry for the given token (no-op if not present).
    void unregister(std::uint32_t token) {
        map_.erase(token);
    }

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------

    /// Look up the primary ConnectionFsm for a route token.
    /// @return Non-null pointer if found; nullptr if the token is unknown.
    [[nodiscard]] ConnectionFsm* find_primary(std::uint32_t token) const noexcept {
        auto it = map_.find(token);
        return (it != map_.end()) ? it->second : nullptr;
    }

    /// Number of pending (unpaired) route tokens.
    [[nodiscard]] std::size_t size() const noexcept { return map_.size(); }

    /// True if no pending route tokens are registered.
    [[nodiscard]] bool empty() const noexcept { return map_.empty(); }

private:
    std::unordered_map<std::uint32_t, ConnectionFsm*> map_;
};

}  // namespace pvpgn::domain::connection
