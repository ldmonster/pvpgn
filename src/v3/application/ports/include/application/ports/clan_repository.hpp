// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file clan_repository.hpp
/// Persistence port for the `social::Clan` aggregate.
///
/// The repository is the *only* seam through which the application
/// layer reads or writes clans. Implementations live in `infra/`.

#include <cstddef>
#include <functional>
#include <optional>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "domain/social/clan.hpp"

namespace pvpgn::application::ports {

class IClanRepository {
public:
    virtual ~IClanRepository() = default;

    /// Look up a clan by primary key.
    virtual core::Result<domain::social::Clan>
    find_by_id(domain::ClanId id) const = 0;

    /// Look up a clan by tag (case-insensitive). The tag is the
    /// 2..4 character identifier like "[TAG]".
    virtual core::Result<domain::social::Clan>
    find_by_tag(std::string_view tag) const = 0;

    /// Find which clan (if any) contains the given account.
    virtual core::Result<domain::social::Clan>
    find_by_member(domain::AccountId account_id) const = 0;

    /// Upsert. Implementations are expected to be idempotent on
    /// repeated `save()` of the same logical state.
    virtual core::Status<>
    save(const domain::social::Clan& clan) = 0;

    /// Remove. Returns `NotFound` if the id doesn't exist.
    virtual core::Status<>
    remove(domain::ClanId id) = 0;

    /// Iterate over all clans, applying predicate. Early exit on
    /// predicate returning false.
    virtual void
    forEach(std::function<bool(const domain::social::Clan&)> predicate) const = 0;

    virtual std::size_t size() const noexcept = 0;
};

}  // namespace pvpgn::application::ports
