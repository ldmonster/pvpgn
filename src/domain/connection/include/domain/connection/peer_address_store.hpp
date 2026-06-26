// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file peer_address_store.hpp
/// Port: per-account peer (remote) IP address registry.
///
/// Some protocols need to report or relay a connected client's network address
/// to other clients — e.g. Westwood Online USERIP (report a user's IP) and
/// STARTG (hand each player the peers' IPs for the P2P game). The transport
/// (TcpSession) knows its own remote endpoint, but a protocol FSM handling one
/// connection cannot reach another connection's address; this store bridges that
/// gap by mapping a logged-in AccountId to its peer IP string.
///
/// Kept as a tiny, protocol-agnostic connection concern (like the session
/// registry) rather than burdening the Account aggregate with transport detail.

#include <optional>
#include <string>

#include "domain/shared/ids.hpp"

namespace pvpgn::domain::connection {

class IPeerAddressStore {
public:
    virtual ~IPeerAddressStore() = default;

    /// Record (or replace) the peer IP for @p account.
    virtual void set(domain::AccountId account, std::string address) = 0;

    /// Look up the peer IP for @p account, or nullopt if not registered.
    [[nodiscard]] virtual std::optional<std::string>
    get(domain::AccountId account) const = 0;

    /// Drop @p account's entry (on disconnect). No-op if absent.
    virtual void remove(domain::AccountId account) = 0;
};

}  // namespace pvpgn::domain::connection
