// SPDX-License-Identifier: GPL-2.0-or-later

/// @file sqlite_account_repository_test.cpp
/// Catch2 unit tests for SQLiteAccountRepository using an in-memory SQLite DB.

#include <catch2/catch_test_macros.hpp>

#include "domain/identity/account.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"
#include "infra/sqlite/account_repository.hpp"
#include "infra/sqlite/connection.hpp"
#include "infra/migrations/migration_runner.hpp"
#include "infra/migrations/all_migrations.hpp"

namespace pvpgn::infra::sqlite {

namespace {

/// Create an in-memory SQLite connection with the full schema applied.
std::shared_ptr<SQLiteConnection> make_in_memory_db() {
    auto conn = std::make_shared<SQLiteConnection>(":memory:");

    // Run all migrations to set up the schema
    migrations::MigrationRunner runner(
        [&conn](std::string_view sql) { return conn->exec(sql); },
        [&conn]() -> std::optional<std::uint32_t> {
            std::optional<std::uint32_t> version;
            conn->query(
                "SELECT MAX(version) FROM _schema_migrations",
                [&version](const Row& row) {
                    if (!row.is_null(0)) {
                        version = static_cast<std::uint32_t>(row.get_int(0));
                    }
                    return false;
                });
            return version;
        });

    runner.ensure_migration_table();
    runner.migrate_to_latest(migrations::get_all_migrations());

    return conn;
}

/// Build a test account with the given id and username.
domain::identity::Account make_test_account(uint32_t id, std::string_view username) {
    auto name_result = domain::UserName::parse(username);
    REQUIRE(name_result.has_value());

    return domain::identity::Account::rehydrate(
        domain::AccountId{id},
        std::move(name_result.value()),
        domain::BNHash{},
        domain::Locale::parse_or_default("enUS"),
        domain::identity::CommandGroupMask{},
        std::nullopt,
        false,
        false);
}

}  // namespace

TEST_CASE("SQLiteAccountRepository: save_and_find_by_name", "[infra][sqlite]") {
    auto conn = make_in_memory_db();
    SQLiteAccountRepository repo(conn);

    auto account = make_test_account(1, "TestUser");
    auto save_result = repo.save(account);
    REQUIRE(save_result.has_value());

    auto find_result = repo.find_by_name(domain::UserName::parse("TestUser").value());
    REQUIRE(find_result.has_value());
    REQUIRE(find_result.value().name().display() == "TestUser");
    REQUIRE(find_result.value().id().value() == 1u);
}

TEST_CASE("SQLiteAccountRepository: find_nonexistent_returns_error", "[infra][sqlite]") {
    auto conn = make_in_memory_db();
    SQLiteAccountRepository repo(conn);

    auto find_result = repo.find_by_name(domain::UserName::parse("NoSuchUser").value());
    REQUIRE_FALSE(find_result.has_value());
    REQUIRE(find_result.error().code() == core::StatusCode::NotFound);
}

TEST_CASE("SQLiteAccountRepository: save_updates_existing", "[infra][sqlite]") {
    auto conn = make_in_memory_db();
    SQLiteAccountRepository repo(conn);

    // Save initial account
    auto account = make_test_account(42, "UpdateMe");
    REQUIRE(repo.save(account).has_value());

    // Verify initial state
    auto first = repo.find_by_name(domain::UserName::parse("UpdateMe").value());
    REQUIRE(first.has_value());
    REQUIRE_FALSE(first.value().is_locked());

    // Save again with locked=true (rehydrate with locked flag)
    auto updated = domain::identity::Account::rehydrate(
        domain::AccountId{42},
        domain::UserName::parse("UpdateMe").value(),
        domain::BNHash{},
        domain::Locale::parse_or_default("enUS"),
        domain::identity::CommandGroupMask{},
        std::nullopt,
        true,   // locked
        false);
    REQUIRE(repo.save(updated).has_value());

    // Verify update was applied
    auto second = repo.find_by_name(domain::UserName::parse("UpdateMe").value());
    REQUIRE(second.has_value());
    REQUIRE(second.value().is_locked());
}

TEST_CASE("SQLiteAccountRepository: find_all_returns_all_saved", "[infra][sqlite]") {
    auto conn = make_in_memory_db();
    SQLiteAccountRepository repo(conn);

    REQUIRE(repo.save(make_test_account(1, "Alice")).has_value());
    REQUIRE(repo.save(make_test_account(2, "Bob")).has_value());
    REQUIRE(repo.save(make_test_account(3, "Charlie")).has_value());

    REQUIRE(repo.size() == 3u);

    std::size_t count = 0;
    repo.forEach([&count](const domain::identity::Account&) {
        ++count;
        return true;  // continue
    });
    REQUIRE(count == 3u);
}

TEST_CASE("SQLiteAccountRepository: remove_deletes_account", "[infra][sqlite]") {
    auto conn = make_in_memory_db();
    SQLiteAccountRepository repo(conn);

    auto account = make_test_account(99, "ToDelete");
    REQUIRE(repo.save(account).has_value());

    // Verify it exists
    REQUIRE(repo.find_by_name(domain::UserName::parse("ToDelete").value()).has_value());

    // Remove by ID
    auto remove_result = repo.remove(domain::AccountId{99});
    REQUIRE(remove_result.has_value());

    // Verify it's gone
    auto find_result = repo.find_by_name(domain::UserName::parse("ToDelete").value());
    REQUIRE_FALSE(find_result.has_value());
}

TEST_CASE("SQLiteAccountRepository: find_by_id_returns_account", "[infra][sqlite]") {
    auto conn = make_in_memory_db();
    SQLiteAccountRepository repo(conn);

    auto account = make_test_account(7, "FindById");
    REQUIRE(repo.save(account).has_value());

    auto find_result = repo.find_by_id(domain::AccountId{7});
    REQUIRE(find_result.has_value());
    REQUIRE(find_result.value().id().value() == 7u);
    REQUIRE(find_result.value().name().display() == "FindById");
}

TEST_CASE("SQLiteAccountRepository: size_reflects_saved_accounts", "[infra][sqlite]") {
    auto conn = make_in_memory_db();
    SQLiteAccountRepository repo(conn);

    REQUIRE(repo.size() == 0u);

    REQUIRE(repo.save(make_test_account(1, "One")).has_value());
    REQUIRE(repo.size() == 1u);

    REQUIRE(repo.save(make_test_account(2, "Two")).has_value());
    REQUIRE(repo.size() == 2u);
}

}  // namespace pvpgn::infra::sqlite
