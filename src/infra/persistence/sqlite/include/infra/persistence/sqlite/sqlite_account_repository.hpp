// SPDX-License-Identifier: GPL-2.0-or-later
#include "domain/identity/ports.hpp"
#pragma once

// R316: This implementation has been superseded by infra/sqlite/SQLiteAccountRepository.
// Do NOT include this header in new code.
// Use: #include "infra/sqlite/account_repository.hpp"
#error "infra/persistence/sqlite is deprecated. Use infra/sqlite instead (pvpgn_infra_sqlite)."

class SqliteDatabase;

/// SQLite-backed implementation of IAccountRepository.
class SqliteAccountRepository : public domain::identity::IAccountRepository {
public:
    explicit SqliteAccountRepository(const std::filesystem::path& db_path);
    ~SqliteAccountRepository();

    // Non-copyable, movable
    SqliteAccountRepository(const SqliteAccountRepository&) = delete;
    SqliteAccountRepository& operator=(const SqliteAccountRepository&) = delete;
    SqliteAccountRepository(SqliteAccountRepository&&) noexcept;
    SqliteAccountRepository& operator=(SqliteAccountRepository&&) noexcept;

    /// Initialize schema (create tables if not exist).
    core::Result<void, core::Error> initialize();

    core::Result<domain::identity::Account, core::Error>
        find_by_name(std::string_view name) override;

    core::Result<domain::identity::Account, core::Error>
        find_by_id(uint32_t id) override;

    core::Result<void, core::Error>
        save(const domain::identity::Account& account) override;

    core::Result<void, core::Error>
        remove(std::string_view name) override;

    core::Result<bool, core::Error>
        exists(std::string_view name) override;

    core::Result<std::vector<domain::identity::Account>, core::Error>
        list_online() override;

    core::Result<uint32_t, core::Error>
        count() override;

private:
    std::unique_ptr<SqliteDatabase> db_;
};

} // namespace pvpgn::infra::persistence::sqlite
