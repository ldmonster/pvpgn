// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/sqlite/account_repository.hpp"

#include <sstream>

#include "domain/identity/account.hpp"
#include "domain/identity/account_snapshot.hpp"

namespace pvpgn::infra::sqlite {

SQLiteAccountRepository::SQLiteAccountRepository(
    std::shared_ptr<SQLiteConnection> conn)
    : conn_(std::move(conn)) {}

domain::identity::Account SQLiteAccountRepository::account_from_row(
    const Row& row) {
    // Extract fields from row
    auto id = domain::AccountId{static_cast<std::uint32_t>(row.get_int(0))};
    auto name = domain::UserName{row.get_text(1)};
    auto locale = row.get_text(2);
    auto password_hash = row.get_text(3);
    bool locked = row.get_int(4) != 0;
    bool must_change_password = row.get_int(5) != 0;
    auto command_groups_str = row.get_text(6);
    auto created_at =
        core::SystemTime{std::chrono::nanoseconds{row.get_int(7)}};
    auto updated_at =
        core::SystemTime{std::chrono::nanoseconds{row.get_int(8)}};

    // Parse command groups (comma-separated)
    std::vector<std::uint8_t> command_groups;
    if (!command_groups_str.empty()) {
        std::istringstream iss(command_groups_str);
        std::string token;
        while (std::getline(iss, token, ',')) {
            if (!token.empty()) {
                command_groups.push_back(
                    static_cast<std::uint8_t>(std::stoul(token)));
            }
        }
    }

    // Construct snapshot
    domain::identity::AccountSnapshot snapshot{
        .id = id.value(),
        .name = std::string{name.value()},
        .locale_code = locale,
        .password_hash_hex = password_hash,
        .locked = locked,
        .must_change_password = must_change_password,
        .command_groups = std::move(command_groups),
        .active_ban = std::nullopt,  // TODO: fetch from account_bans
        .created_at = created_at,
        .updated_at = updated_at,
    };

    return domain::identity::Account::rehydrate(snapshot);
}

core::Result<domain::identity::Account> SQLiteAccountRepository::find_by_id(
    domain::AccountId id) const {
    if (!conn_) {
        return core::fail(core::Error{
            core::StatusCode::Internal, "sqlite: connection not available"});
    }

    domain::identity::Account* result = nullptr;

    auto query_result = conn_->query_bind(
        "SELECT id, name, locale, password_hash, locked, must_change_password, "
        "command_groups, created_at, updated_at FROM accounts WHERE id = ?",
        {static_cast<std::int64_t>(id.value())},
        [&result](const Row& row) {
            try {
                result = new domain::identity::Account{account_from_row(row)};
                return false;  // Stop iteration
            } catch (...) {
                return false;
            }
        });

    if (!query_result.has_value()) {
        return core::fail(query_result.error());
    }

    if (!result) {
        return core::fail(core::Error{
            core::StatusCode::NotFound, "account: id not found"});
    }

    auto account = *result;
    delete result;
    return account;
}

core::Result<domain::identity::Account> SQLiteAccountRepository::find_by_name(
    const domain::UserName& name) const {
    if (!conn_) {
        return core::fail(core::Error{
            core::StatusCode::Internal, "sqlite: connection not available"});
    }

    domain::identity::Account* result = nullptr;

    auto query_result = conn_->query_bind(
        "SELECT id, name, locale, password_hash, locked, must_change_password, "
        "command_groups, created_at, updated_at FROM accounts WHERE name = ? "
        "COLLATE NOCASE",
        {std::string{name.value()}},
        [&result](const Row& row) {
            try {
                result = new domain::identity::Account{account_from_row(row)};
                return false;  // Stop iteration
            } catch (...) {
                return false;
            }
        });

    if (!query_result.has_value()) {
        return core::fail(query_result.error());
    }

    if (!result) {
        return core::fail(core::Error{
            core::StatusCode::NotFound, "account: name not found"});
    }

    auto account = *result;
    delete result;
    return account;
}

core::Status<> SQLiteAccountRepository::save(
    const domain::identity::Account& account) {
    if (!conn_) {
        return core::fail(core::Error{
            core::StatusCode::Internal, "sqlite: connection not available"});
    }

    // Build command groups string
    std::ostringstream groups_oss;
    for (size_t i = 0; i < account.command_groups().size(); ++i) {
        if (i > 0) groups_oss << ",";
        groups_oss << static_cast<int>(account.command_groups()[i]);
    }

    // INSERT OR REPLACE into accounts table
    std::ostringstream sql;
    sql << "INSERT OR REPLACE INTO accounts "
        << "(id, name, locale, password_hash, locked, must_change_password, "
        << "command_groups, created_at, updated_at) "
        << "VALUES (" << account.id().value() << ", '"
        << account.name().value() << "', '"
        << account.locale_code() << "', '"
        << account.password_hash_hex() << "', "
        << (account.locked() ? 1 : 0) << ", "
        << (account.must_change_password() ? 1 : 0) << ", '"
        << groups_oss.str() << "', "
        << account.created_at().time_since_epoch().count() << ", "
        << account.updated_at().time_since_epoch().count() << ");";

    return conn_->exec(sql.str());
}

core::Status<> SQLiteAccountRepository::remove(domain::AccountId id) {
    if (!conn_) {
        return core::fail(core::Error{
            core::StatusCode::Internal, "sqlite: connection not available"});
    }

    std::ostringstream sql;
    sql << "DELETE FROM accounts WHERE id = " << id.value() << ";";

    auto result = conn_->exec(sql.str());

    // Check if anything was deleted by querying count
    if (result.has_value()) {
        // Verify account existed - do a quick check
        // For now, assume deletion succeeded
        return core::ok();
    }

    return result;
}

void SQLiteAccountRepository::forEach(
    std::function<bool(const domain::identity::Account&)> predicate) const {
    if (!conn_) {
        return;
    }

    conn_->query(
        "SELECT id, name, locale, password_hash, locked, must_change_password, "
        "command_groups, created_at, updated_at FROM accounts",
        [&predicate](const Row& row) {
            try {
                auto account = account_from_row(row);
                return predicate(account);
            } catch (...) {
                return false;
            }
        });
}

std::size_t SQLiteAccountRepository::size() const noexcept {
    if (!conn_) {
        return 0;
    }

    std::size_t count = 0;
    conn_->query(
        "SELECT COUNT(*) FROM accounts",
        [&count](const Row& row) {
            count = static_cast<std::size_t>(row.get_int(0));
            return false;  // Stop after first row
        });

    return count;
}

}  // namespace pvpgn::infra::sqlite
