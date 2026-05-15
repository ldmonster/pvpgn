// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file realm_repository.hpp
/// Persistence port for the `realm::Realm` aggregate.
///
/// The repository is the *only* seam through which the application
/// layer reads or writes realms. Implementations live in `infra/`:
/// in-memory (tests + dev), file-backed (legacy parity), and SQL
/// (production) all satisfy this interface.

#include <cstddef>
#include <cstdint>
#include <functional>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/realm/realm.hpp"

namespace pvpgn::application::ports {

class IRealmRepository {
public:
    virtual ~IRealmRepository() = default;

    /// Look up a realm by primary key. Returns `NotFound` if absent.
    virtual core::Result<domain::realm::Realm>
    find_by_id(std::uint32_t id) const = 0;

    /// Case-insensitive name lookup. Returns `NotFound` if absent.
    virtual core::Result<domain::realm::Realm>
    find_by_name(const std::string& name) const = 0;

    /// Upsert. Implementations are expected to be idempotent on
    /// repeated `save()` of the same logical state.
    virtual core::Status<>
    save(const domain::realm::Realm& realm) = 0;

    /// Remove. Returns `NotFound` if the id doesn't exist.
    virtual core::Status<> remove(std::uint32_t id) = 0;

    /// Iterate over all realms, applying predicate. Early exit on
    /// predicate returning false.
    virtual void
    forEach(std::function<bool(const domain::realm::Realm&)> predicate) const = 0;

    virtual std::size_t size() const noexcept = 0;
};

}  // namespace pvpgn::application::ports
