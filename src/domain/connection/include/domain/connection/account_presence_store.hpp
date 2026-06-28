// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file account_presence_store.hpp
/// Port: per-account live-presence registry (client product + away/DND state).
///
/// The friend list (SID_FRIENDSLIST / SID_FRIENDINFO) reports, for each online
/// friend, that friend's product tag (clienttag) and away/DND flags — data the
/// original sources from the friend's live connection (conn_get_clienttag /
/// conn_get_awaystr / conn_get_dndstr). A protocol FSM handling one connection
/// cannot reach another connection's state, so this store bridges the gap: each
/// logged-in BnetFsm publishes its own clienttag/away/DND here, and ListFriends
/// reads it back for a friend.
///
/// Kept as a tiny, protocol-agnostic connection concern (like the peer-address
/// store and the session registry) rather than burdening the Account aggregate
/// with transient connection state. Entries are created on login and dropped on
/// disconnect, so a lookup that misses simply means "not currently online".

#include <cstdint>
#include <optional>

#include "domain/shared/ids.hpp"

namespace pvpgn::domain::connection {

struct AccountPresence {
    std::uint32_t client_tag = 0;  ///< big-endian-packed product tag (e.g. STAR)
    bool          away       = false;
    bool          dnd        = false;
};

class IAccountPresenceStore {
public:
    virtual ~IAccountPresenceStore() = default;

    /// Record (or replace) @p account's product tag, creating the entry. Clears
    /// any stale away/DND flags so a fresh login starts available.
    virtual void set_client_tag(domain::AccountId account,
                                std::uint32_t client_tag) = 0;

    /// Set @p account's away flag (upserts the entry if absent).
    virtual void set_away(domain::AccountId account, bool away) = 0;

    /// Set @p account's DND flag (upserts the entry if absent).
    virtual void set_dnd(domain::AccountId account, bool dnd) = 0;

    /// Look up @p account's presence, or nullopt if not registered (offline).
    [[nodiscard]] virtual std::optional<AccountPresence>
    get(domain::AccountId account) const = 0;

    /// Drop @p account's entry (on disconnect). No-op if absent.
    virtual void remove(domain::AccountId account) = 0;
};

}  // namespace pvpgn::domain::connection
