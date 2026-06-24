// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file realm_repository.hpp
/// A single, driver-parameterized realm repository over `IDbDriver`. One
/// implementation for sqlite/mysql/postgres, backend chosen at composition time.

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/realm/ports.hpp"
#include "domain/realm/realm.hpp"
#include "infra/persistence/sql_builder/db_driver.hpp"

namespace pvpgn::infra::persistence {

/// `IRealmRepository` implemented over the backend-agnostic `IDbDriver`.
///
/// Schema (table `realms`):
///   id          INTEGER PRIMARY KEY,
///   name        TEXT,
///   description TEXT,
///   active      INTEGER   -- 0/1
///
/// Per-realm characters are persisted by a separate character store; this
/// repository owns only the realm record.
class SqlRealmRepository final : public domain::realm::IRealmRepository {
public:
    explicit SqlRealmRepository(std::shared_ptr<IDbDriver> driver)
        : driver_(std::move(driver)) {}

    [[nodiscard]] core::Result<domain::realm::Realm, core::Error>
    find_by_id(std::uint32_t id) const override;

    [[nodiscard]] core::Result<domain::realm::Realm, core::Error>
    find_by_name(const std::string& name) const override;

    core::Result<void, core::Error> save(
        const domain::realm::Realm& realm) override;

    core::Result<void, core::Error> remove(std::uint32_t id) override;

    void forEach(
        std::function<bool(const domain::realm::Realm&)> pred) const override;

    [[nodiscard]] std::size_t size() const noexcept override;

private:
    /// Rehydrate a Realm from a row, or nullopt if the row is malformed
    /// (e.g. a name that fails the aggregate's validation).
    static std::optional<domain::realm::Realm> realm_from_row(const DbRow& row);

    std::shared_ptr<IDbDriver> driver_;
};

}  // namespace pvpgn::infra::persistence
