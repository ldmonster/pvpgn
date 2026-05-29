// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/persistence/sqlite/sqlite_account_repository.hpp"
#include "infra/persistence/sqlite/sqlite_database.hpp"
#include "domain/identity/account.hpp"
#include "domain/shared/user_name.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/ids.hpp"
#include "core/error.hpp"
#include <utility>

namespace pvpgn::infra::persistence::sqlite {

SqliteAccountRepository::SqliteAccountRepository(const std::filesystem::path& db_path)
    : db_(std::make_unique<SqliteDatabase>(db_path)) {}

SqliteAccountRepository::~SqliteAccountRepository() = default;

SqliteAccountRepository::SqliteAccountRepository(SqliteAccountRepository&&) noexcept =
    default;

SqliteAccountRepository& SqliteAccountRepository::operator=(
    SqliteAccountRepository&&) noexcept = default;

core::Result<void, core::Error> SqliteAccountRepository::initialize() {
    if (auto res = db_->open(); !res) {
        return res;
    }

    // Create accounts table
    const char* schema = R"(
        CREATE TABLE IF NOT EXISTS accounts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            email TEXT,
            created_at INTEGER NOT NULL,
            last_login INTEGER,
            is_online INTEGER NOT NULL DEFAULT 0,
            flags INTEGER NOT NULL DEFAULT 0,
            attributes TEXT
        );
        CREATE INDEX IF NOT EXISTS idx_accounts_name ON accounts(name);
    )";

    return db_->execute(schema);
}

core::Result<domain::identity::Account, core::Error> SqliteAccountRepository::find_by_name(
    std::string_view name) {
    if (!db_->is_open()) {
        return core::fail(core::Error{core::StatusCode::FailedPrecondition,
                                       "Database not open"});
    }

    auto stmt_res = db_->prepare(
        "SELECT id, name, password_hash, flags FROM accounts WHERE name = ? LIMIT 1");
    if (!stmt_res) {
        return core::fail(stmt_res.error());
    }

    auto& stmt = stmt_res.value();
    if (auto res = stmt.bind_text(1, name); !res) {
        return core::fail(res.error());
    }

    if (!stmt.step()) {
        return core::fail(core::Error{core::StatusCode::NotFound,
                                       std::string("Account not found: ") +
                                           std::string(name)});
    }

    uint32_t id = stmt.column_int(0);
    std::string account_name = stmt.column_text(1);
    std::string password_hash = stmt.column_text(2);

    // Reconstruct domain objects from stored data
    auto user_name_res = domain::UserName::parse(account_name);
    if (!user_name_res) {
        return core::fail(user_name_res.error());
    }

    // For now, create a minimal Account with rehydrate
    // In production, you'd deserialize all fields from the database
    auto account = domain::identity::Account::rehydrate(
        domain::AccountId{id},
        std::move(user_name_res.value()),
        domain::BNHash::from_bytes(password_hash).value_or(domain::BNHash{}),
        domain::Locale{},  // Default locale
        domain::identity::CommandGroupMask{},
        std::nullopt,  // No ban
        false,         // Not locked
        false          // No password rotation required
    );

    return account;
}

core::Result<domain::identity::Account, core::Error> SqliteAccountRepository::find_by_id(
    uint32_t id) {
    if (!db_->is_open()) {
        return core::fail(core::Error{core::StatusCode::FailedPrecondition,
                                       "Database not open"});
    }

    auto stmt_res = db_->prepare(
        "SELECT id, name, password_hash, flags FROM accounts WHERE id = ? LIMIT 1");
    if (!stmt_res) {
        return core::fail(stmt_res.error());
    }

    auto& stmt = stmt_res.value();
    if (auto res = stmt.bind_int(1, id); !res) {
        return core::fail(res.error());
    }

    if (!stmt.step()) {
        return core::fail(core::Error{core::StatusCode::NotFound,
                                       "Account not found"});
    }

    uint32_t account_id = stmt.column_int(0);
    std::string account_name = stmt.column_text(1);
    std::string password_hash = stmt.column_text(2);

    auto user_name_res = domain::UserName::parse(account_name);
    if (!user_name_res) {
        return core::fail(user_name_res.error());
    }

    auto account = domain::identity::Account::rehydrate(
        domain::AccountId{account_id},
        std::move(user_name_res.value()),
        domain::BNHash::from_bytes(password_hash).value_or(domain::BNHash{}),
        domain::Locale{},
        domain::identity::CommandGroupMask{},
        std::nullopt,
        false,
        false);

    return account;
}

core::Result<void, core::Error> SqliteAccountRepository::save(
    const domain::identity::Account& account) {
    if (!db_->is_open()) {
        return core::fail(core::Error{core::StatusCode::FailedPrecondition,
                                       "Database not open"});
    }

    // Check if account exists
    auto exists_res = exists(account.name().canonical());
    if (!exists_res) {
        return core::fail(exists_res.error());
    }

    if (exists_res.value()) {
        // Update existing
        auto stmt_res = db_->prepare(
            "UPDATE accounts SET password_hash = ?, flags = ? WHERE id = ?");
        if (!stmt_res) {
            return core::fail(stmt_res.error());
        }

        auto& stmt = stmt_res.value();
        auto hash_bytes = account.password_hash1().bytes();
        std::string hash_str(reinterpret_cast<const char*>(hash_bytes.data()), hash_bytes.size());
        if (auto res = stmt.bind_text(1, hash_str); !res) {
            return core::fail(res.error());
        }
        if (auto res = stmt.bind_int(2, 0); !res) {  // flags placeholder
            return core::fail(res.error());
        }
        if (auto res = stmt.bind_int(3, account.id().value()); !res) {
            return core::fail(res.error());
        }

        stmt.step();
    } else {
        // Insert new
        auto stmt_res = db_->prepare(
            "INSERT INTO accounts (name, password_hash, created_at, flags) "
            "VALUES (?, ?, ?, ?)");
        if (!stmt_res) {
            return core::fail(stmt_res.error());
        }

        auto& stmt = stmt_res.value();
        auto hash_bytes = account.password_hash1().bytes();
        std::string hash_str(reinterpret_cast<const char*>(hash_bytes.data()), hash_bytes.size());
        if (auto res = stmt.bind_text(1, account.name().canonical()); !res) {
            return core::fail(res.error());
        }
        if (auto res = stmt.bind_text(2, hash_str); !res) {
            return core::fail(res.error());
        }
        if (auto res = stmt.bind_int(3, 0); !res) {  // created_at placeholder
            return core::fail(res.error());
        }
        if (auto res = stmt.bind_int(4, 0); !res) {  // flags placeholder
            return core::fail(res.error());
        }

        stmt.step();
    }

    return {};
}

core::Result<void, core::Error> SqliteAccountRepository::remove(std::string_view name) {
    if (!db_->is_open()) {
        return core::fail(core::Error{core::StatusCode::FailedPrecondition,
                                       "Database not open"});
    }

    auto stmt_res = db_->prepare("DELETE FROM accounts WHERE name = ?");
    if (!stmt_res) {
        return core::fail(stmt_res.error());
    }

    auto& stmt = stmt_res.value();
    if (auto res = stmt.bind_text(1, name); !res) {
        return core::fail(res.error());
    }

    stmt.step();
    return {};
}

core::Result<bool, core::Error> SqliteAccountRepository::exists(std::string_view name) {
    if (!db_->is_open()) {
        return core::fail(core::Error{core::StatusCode::FailedPrecondition,
                                       "Database not open"});
    }

    auto stmt_res = db_->prepare("SELECT 1 FROM accounts WHERE name = ? LIMIT 1");
    if (!stmt_res) {
        return core::fail(stmt_res.error());
    }

    auto& stmt = stmt_res.value();
    if (auto res = stmt.bind_text(1, name); !res) {
        return core::fail(res.error());
    }

    return stmt.step();
}

core::Result<std::vector<domain::identity::Account>, core::Error>
SqliteAccountRepository::list_online() {
    if (!db_->is_open()) {
        return core::fail(core::Error{core::StatusCode::FailedPrecondition,
                                       "Database not open"});
    }

    auto stmt_res = db_->prepare(
        "SELECT id, name, password_hash, flags FROM accounts WHERE is_online = 1");
    if (!stmt_res) {
        return core::fail(stmt_res.error());
    }

    std::vector<domain::identity::Account> accounts;
    auto& stmt = stmt_res.value();

    while (stmt.step()) {
        uint32_t id = stmt.column_int(0);
        std::string account_name = stmt.column_text(1);
        std::string password_hash = stmt.column_text(2);

        auto user_name_res = domain::UserName::parse(account_name);
        if (!user_name_res) {
            continue;  // Skip invalid names
        }

        auto account = domain::identity::Account::rehydrate(
            domain::AccountId{id},
            std::move(user_name_res.value()),
            domain::BNHash::from_bytes(password_hash).value_or(domain::BNHash{}),
            domain::Locale{},
            domain::identity::CommandGroupMask{},
            std::nullopt,
            false,
            false);

        accounts.push_back(std::move(account));
    }

    return accounts;
}

core::Result<uint32_t, core::Error> SqliteAccountRepository::count() {
    if (!db_->is_open()) {
        return core::fail(core::Error{core::StatusCode::FailedPrecondition,
                                       "Database not open"});
    }

    auto stmt_res = db_->prepare("SELECT COUNT(*) FROM accounts");
    if (!stmt_res) {
        return core::fail(stmt_res.error());
    }

    auto& stmt = stmt_res.value();
    if (!stmt.step()) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                       "Failed to count accounts"});
    }

    return static_cast<uint32_t>(stmt.column_int(0));
}

} // namespace pvpgn::infra::persistence::sqlite
