// SPDX-License-Identifier: GPL-2.0-or-later
#include "infra/persistence/account_ban_repository.hpp"

#include <chrono>

namespace pvpgn::infra::persistence {

namespace {

/// Serialise a SystemTime to unix epoch seconds (the on-disk representation).
std::int64_t to_epoch_seconds(core::SystemTime t) {
    return std::chrono::duration_cast<std::chrono::seconds>(
               t.time_since_epoch())
        .count();
}

/// Reconstruct a SystemTime from unix epoch seconds.
core::SystemTime from_epoch_seconds(std::int64_t s) {
    return core::SystemTime{} + std::chrono::seconds{s};
}

constexpr std::string_view kSelectCols =
    "account_id, banned_by, reason, banned_at, expires_at";

}  // namespace

domain::moderation::AccountBan SqlAccountBanRepository::ban_from_row(
    const DbRow& row) {
    // Column order matches kSelectCols.
    domain::moderation::AccountBan ban;
    ban.banned_account =
        domain::AccountId{static_cast<std::uint32_t>(row.get_int(0))};
    ban.banned_by =
        domain::AccountId{static_cast<std::uint32_t>(row.get_int(1))};
    ban.reason    = row.get_text(2);
    ban.banned_at = from_epoch_seconds(row.get_int(3));
    if (!row.is_null(4)) {
        ban.expires_at = from_epoch_seconds(row.get_int(4));
    }
    return ban;
}

core::Result<std::optional<domain::moderation::AccountBan>>
SqlAccountBanRepository::find_active_ban(domain::AccountId account_id,
                                         core::SystemTime now) const {
    if (!driver_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "persistence: driver not available"});
    }

    std::optional<domain::moderation::AccountBan> found;
    std::string sql = "SELECT ";
    sql.append(kSelectCols);
    sql += " FROM account_bans WHERE account_id = ?";

    auto q = driver_->query_bind(
        sql, {static_cast<std::int64_t>(account_id.value())},
        [&found](const DbRow& row) {
            found = ban_from_row(row);
            return false;  // at most one row (account_id is the primary key)
        });
    if (!q.has_value()) {
        return core::fail(q.error());
    }

    // A stored-but-expired ban is not an *active* ban.
    if (found && !found->active_at(now)) {
        found.reset();
    }
    return found;
}

core::Status<> SqlAccountBanRepository::add_ban(
    const domain::moderation::AccountBan& ban) {
    if (!driver_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "persistence: driver not available"});
    }

    // INSERT OR REPLACE keeps a single active ban per account (re-banning
    // overwrites). The dialect layer rewrites this to ON CONFLICT for Postgres.
    const char* sql =
        "INSERT OR REPLACE INTO account_bans "
        "(account_id, banned_by, reason, banned_at, expires_at) "
        "VALUES (?, ?, ?, ?, ?)";

    DbParamValue expires = ban.expires_at.has_value()
                               ? DbParamValue{to_epoch_seconds(*ban.expires_at)}
                               : DbParamValue{nullptr};

    return driver_->query_bind(
        sql,
        {static_cast<std::int64_t>(ban.banned_account.value()),
         static_cast<std::int64_t>(ban.banned_by.value()), ban.reason,
         to_epoch_seconds(ban.banned_at), expires},
        [](const DbRow&) { return false; });  // write: no rows expected
}

core::Status<> SqlAccountBanRepository::remove_ban(
    domain::AccountId account_id) {
    if (!driver_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "persistence: driver not available"});
    }
    return driver_->query_bind(
        "DELETE FROM account_bans WHERE account_id = ?",
        {static_cast<std::int64_t>(account_id.value())},
        [](const DbRow&) { return false; });
}

void SqlAccountBanRepository::for_each(
    std::function<bool(const domain::moderation::AccountBan&)> predicate)
    const {
    if (!driver_) return;

    std::string sql = "SELECT ";
    sql.append(kSelectCols);
    sql += " FROM account_bans";

    (void)driver_->query(sql, [&predicate](const DbRow& row) {
        return predicate(ban_from_row(row));
    });
}

}  // namespace pvpgn::infra::persistence
