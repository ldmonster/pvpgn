// SPDX-License-Identifier: GPL-2.0-or-later
#include "infra/persistence/ladder_repository.hpp"

#include <optional>
#include <string>

namespace pvpgn::infra::persistence {

namespace {
bool no_rows(const DbRow&) { return false; }
constexpr std::string_view kCols =
    "account_id, rating, wins, losses, disconnects";
}  // namespace

domain::ladder::LadderEntry SqlLadderRepository::entry_from_row(
    const DbRow& row) {
    domain::ladder::LadderEntry e;
    e.account = domain::AccountId{static_cast<std::uint32_t>(row.get_int(0))};
    e.rating  = static_cast<std::int32_t>(row.get_int(1));
    e.wins        = static_cast<std::uint32_t>(row.get_int(2));
    e.losses      = static_cast<std::uint32_t>(row.get_int(3));
    e.disconnects = static_cast<std::uint32_t>(row.get_int(4));
    return e;
}

core::Result<std::uint32_t, core::Error> SqlLadderRepository::get_rank(
    domain::AccountId account_id) {
    if (!driver_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "persistence: driver not available"});
    }

    // 1) Resolve the account's rating (and confirm it is on the ladder).
    std::optional<std::int64_t> rating;
    auto q1 = driver_->query_bind(
        "SELECT rating FROM ladder WHERE account_id = ?",
        {static_cast<std::int64_t>(account_id.value())},
        [&rating](const DbRow& row) {
            rating = row.get_int(0);
            return false;
        });
    if (!q1.has_value()) return core::fail(q1.error());
    if (!rating) {
        return core::fail(
            core::Error{core::StatusCode::NotFound, "ladder: account not found"});
    }

    // 2) Rank = 1 + (number of accounts rated strictly higher).
    std::int64_t higher = 0;
    auto q2 = driver_->query_bind(
        "SELECT COUNT(*) FROM ladder WHERE rating > ?", {*rating},
        [&higher](const DbRow& row) {
            higher = row.get_int(0);
            return false;
        });
    if (!q2.has_value()) return core::fail(q2.error());
    return static_cast<std::uint32_t>(higher + 1);
}

core::Result<void, core::Error> SqlLadderRepository::save_entry(
    const domain::ladder::LadderEntry& entry) {
    if (!driver_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "persistence: driver not available"});
    }
    return driver_->query_bind(
        "INSERT OR REPLACE INTO ladder "
        "(account_id, rating, wins, losses, disconnects) "
        "VALUES (?, ?, ?, ?, ?)",
        {static_cast<std::int64_t>(entry.account.value()),
         static_cast<std::int64_t>(entry.rating),
         static_cast<std::int64_t>(entry.wins),
         static_cast<std::int64_t>(entry.losses),
         static_cast<std::int64_t>(entry.disconnects)},
        no_rows);
}

core::Result<std::vector<domain::ladder::LadderEntry>, core::Error>
SqlLadderRepository::get_top_n(std::uint32_t n) {
    if (!driver_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "persistence: driver not available"});
    }
    std::vector<domain::ladder::LadderEntry> entries;
    std::string sql = "SELECT ";
    sql.append(kCols);
    sql += " FROM ladder ORDER BY rating DESC LIMIT ?";

    auto q = driver_->query_bind(
        sql, {static_cast<std::int64_t>(n)},
        [&entries](const DbRow& row) {
            entries.push_back(entry_from_row(row));
            return true;
        });
    if (!q.has_value()) return core::fail(q.error());
    return entries;
}

}  // namespace pvpgn::infra::persistence
