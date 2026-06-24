// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
//
// domain/realm/ports.hpp — Abstract ports (interfaces) for the realm bounded context.
// Implementations live in src/infra/<tech>/ and src/integration/<binding>/.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "core/result.hpp"
#include "domain/realm/realm.hpp"

namespace pvpgn::domain::realm {

// ---------------------------------------------------------------------------
// IRealmRepository
// ---------------------------------------------------------------------------

/// Port interface for realm storage.
///
/// All mutating operations are idempotent with respect to the realm's
/// identity (id / name pair).  Implementations must be thread-safe.
class IRealmRepository {
public:
    virtual ~IRealmRepository() = default;

    /// Look up a realm by its numeric id.
    /// @returns the Realm on success, or a NotFound error.
    [[nodiscard]] virtual core::Result<Realm, core::Error>
    find_by_id(std::uint32_t id) const = 0;

    /// Look up a realm by its display name (case-insensitive).
    /// @returns the Realm on success, or a NotFound error.
    [[nodiscard]] virtual core::Result<Realm, core::Error>
    find_by_name(const std::string& name) const = 0;

    /// Insert or update a realm record.
    virtual core::Result<void, core::Error>
    save(const Realm& realm) = 0;

    /// Remove the realm with the given id.
    /// @returns NotFound if no such realm exists.
    virtual core::Result<void, core::Error>
    remove(std::uint32_t id) = 0;

    /// Iterate over every stored realm.
    /// The predicate receives each realm by const-ref; returning `false`
    /// stops iteration early.
    virtual void
    forEach(std::function<bool(const Realm&)> pred) const = 0;

    /// Return the number of stored realms.
    [[nodiscard]] virtual std::size_t size() const noexcept = 0;
};

} // namespace pvpgn::domain::realm
