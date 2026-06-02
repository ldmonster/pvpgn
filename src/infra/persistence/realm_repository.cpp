// SPDX-License-Identifier: GPL-2.0-or-later
#include "infra/persistence/realm_repository.hpp"

namespace pvpgn::infra::persistence {

namespace {
constexpr std::string_view kCols = "id, name, description, active";
}  // namespace

std::optional<domain::realm::Realm> SqlRealmRepository::realm_from_row(
    const DbRow& row) {
    // Column order matches kCols.
    auto created = domain::realm::Realm::create(
        static_cast<std::uint32_t>(row.get_int(0)),  // id
        row.get_text(1),                             // name
        row.get_text(2));                            // description
    if (!created.has_value()) {
        return std::nullopt;  // stored name failed aggregate validation
    }
    domain::realm::Realm realm = std::move(created.value());

    // `Realm::create` starts active; reflect the stored flag, then discard the
    // reconstruction events so the rehydrated aggregate has a clean slate.
    if (row.get_int(3) == 0) {
        realm.unregister();
    }
    (void)realm.drain_events();
    return realm;
}

core::Result<domain::realm::Realm, core::Error>
SqlRealmRepository::find_by_id(std::uint32_t id) const {
    if (!driver_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "persistence: driver not available"});
    }
    std::optional<domain::realm::Realm> found;
    std::string sql = "SELECT ";
    sql.append(kCols);
    sql += " FROM realms WHERE id = ?";

    auto q = driver_->query_bind(
        sql, {static_cast<std::int64_t>(id)},
        [&found](const DbRow& row) {
            found = realm_from_row(row);
            return false;  // id is the primary key — at most one row
        });
    if (!q.has_value()) {
        return core::fail(q.error());
    }
    if (!found) {
        return core::fail(
            core::Error{core::StatusCode::NotFound, "realm: id not found"});
    }
    return std::move(*found);
}

core::Result<domain::realm::Realm, core::Error>
SqlRealmRepository::find_by_name(const std::string& name) const {
    if (!driver_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "persistence: driver not available"});
    }
    std::optional<domain::realm::Realm> found;
    std::string sql = "SELECT ";
    sql.append(kCols);
    sql += " FROM realms WHERE name = ? COLLATE NOCASE";

    auto q = driver_->query_bind(
        sql, {name},
        [&found](const DbRow& row) {
            found = realm_from_row(row);
            return false;
        });
    if (!q.has_value()) {
        return core::fail(q.error());
    }
    if (!found) {
        return core::fail(
            core::Error{core::StatusCode::NotFound, "realm: name not found"});
    }
    return std::move(*found);
}

core::Result<void, core::Error> SqlRealmRepository::save(
    const domain::realm::Realm& realm) {
    if (!driver_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "persistence: driver not available"});
    }
    return driver_->query_bind(
        "INSERT OR REPLACE INTO realms (id, name, description, active) "
        "VALUES (?, ?, ?, ?)",
        {static_cast<std::int64_t>(realm.id()), realm.name(),
         realm.description(),
         static_cast<std::int64_t>(realm.active() ? 1 : 0)},
        [](const DbRow&) { return false; });
}

core::Result<void, core::Error> SqlRealmRepository::remove(std::uint32_t id) {
    if (!driver_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "persistence: driver not available"});
    }
    return driver_->query_bind("DELETE FROM realms WHERE id = ?",
                               {static_cast<std::int64_t>(id)},
                               [](const DbRow&) { return false; });
}

void SqlRealmRepository::forEach(
    std::function<bool(const domain::realm::Realm&)> pred) const {
    if (!driver_) return;
    std::string sql = "SELECT ";
    sql.append(kCols);
    sql += " FROM realms";

    (void)driver_->query(sql, [&pred](const DbRow& row) {
        auto realm = realm_from_row(row);
        if (!realm) return true;  // skip malformed rows, keep iterating
        return pred(*realm);
    });
}

std::size_t SqlRealmRepository::size() const noexcept {
    if (!driver_) return 0;
    std::size_t count = 0;
    (void)driver_->query("SELECT COUNT(*) FROM realms",
                         [&count](const DbRow& row) {
                             count = static_cast<std::size_t>(row.get_int(0));
                             return false;
                         });
    return count;
}

}  // namespace pvpgn::infra::persistence
