// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file realm_list.hpp
/// Application-layer dispatcher for the BNCS realm-list query
/// (CLIENT_REALMLISTREQ / CLIENT_REALMLISTREQ_110).
///
/// Pure C++; no dependency on legacy bnetd globals. The legacy
/// `t_realm` / `realmlist_t` aggregate is hidden behind the
/// `IRealmRepository` port -- the linked half of the bridge in
/// `integration_legacy_bnetd_linked` provides the adapter.

#include <cstdint>
#include <string>
#include <vector>

namespace pvpgn::application::realm {

/// One realm as returned by the application dispatcher.
struct RealmListing {
    std::uint32_t id          = 0;
    std::string   name;
    std::string   description;
    bool          active      = false;
};

/// Port: enumerate the configured realm catalog.
///
/// Legacy adapter walks `realmlist()` from `src/bnetd/realm.cpp`.
class IRealmRepository {
public:
    virtual ~IRealmRepository() = default;

    /// Snapshot of every configured realm (active + inactive).
    /// The dispatcher filters; the repository is just a source.
    virtual std::vector<RealmListing> list_all() const = 0;
};

/// Response to a realm-list query: only active realms, in repository
/// order (legacy semantics).
struct RealmListResponse {
    std::vector<RealmListing> active_entries;
};

/// Filter the repository snapshot down to active realms.
/// Stateless / order-preserving / pure.
RealmListResponse dispatch_realm_list(const IRealmRepository& repo);

} // namespace pvpgn::application::realm
