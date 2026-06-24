// SPDX-License-Identifier: GPL-2.0-or-later

/// @file sql_account_repository_test.cpp
/// Exercises the driver-parameterized
/// `infra::persistence::SqlAccountRepository` over the SQLite `IDbDriver`
/// (in-memory). The same repository runs unchanged over the MySQL/PostgreSQL
/// drivers — this is the SQLite column of the cross-backend matrix.

#include <memory>

#include <catch2/catch_test_macros.hpp>

#include "domain/identity/account.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"

#include "domain/chat/channel.hpp"

#include "infra/persistence/account_repository.hpp"
#include "infra/persistence/channel_repository.hpp"
#include "infra/persistence/repository_factory.hpp"
#include "infra/persistence/sql_builder/sqlite_driver.hpp"
#include "infra/sqlite/connection.hpp"
#include "infra/migrations/migration_runner.hpp"
#include "infra/migrations/all_migrations.hpp"

namespace pvpgn::infra::persistence {

namespace {

/// In-memory SQLite driver with the full schema applied.
std::shared_ptr<IDbDriver> make_sqlite_driver() {
    auto conn = std::make_shared<sqlite::SQLiteConnection>(":memory:");

    migrations::MigrationRunner runner(
        [&conn](std::string_view sql) { return conn->exec(sql); },
        [&conn]() -> std::optional<std::uint32_t> {
            std::optional<std::uint32_t> version;
            (void)conn->query(
                "SELECT MAX(version) FROM _schema_migrations",
                [&version](const sqlite::Row& row) {
                    if (!row.is_null(0)) {
                        version = static_cast<std::uint32_t>(row.get_int(0));
                    }
                    return false;
                });
            return version;
        });
    REQUIRE(runner.ensure_migration_table().has_value());
    REQUIRE(runner.migrate_to_latest(
        migrations::get_all_migrations()).has_value());

    return std::make_shared<SqliteDriver>(std::move(conn));
}

domain::identity::Account make_account(std::uint32_t id, std::string_view name) {
    auto n = domain::UserName::parse(name);
    REQUIRE(n.has_value());
    return domain::identity::Account::rehydrate(
        domain::AccountId{id}, std::move(n.value()), domain::BNHash{},
        domain::Locale::parse_or_default("enUS"),
        domain::identity::CommandGroupMask{}, std::nullopt, false, false);
}

}  // namespace

TEST_CASE("SqlAccountRepository[sqlite]: save / find_by_name / find_by_id",
          "[infra][persistence][sqlite]") {
    SqlAccountRepository repo(make_sqlite_driver());

    REQUIRE(repo.save(make_account(1, "TestUser")).has_value());

    auto by_name = repo.find_by_name(domain::UserName::parse("TestUser").value());
    REQUIRE(by_name.has_value());
    REQUIRE(by_name.value().id().value() == 1u);
    REQUIRE(by_name.value().name().display() == "TestUser");

    auto by_id = repo.find_by_id(domain::AccountId{1});
    REQUIRE(by_id.has_value());
    REQUIRE(by_id.value().name().display() == "TestUser");
}

TEST_CASE("SqlAccountRepository[sqlite]: missing lookup returns NotFound",
          "[infra][persistence][sqlite]") {
    SqlAccountRepository repo(make_sqlite_driver());
    auto r = repo.find_by_name(domain::UserName::parse("Nobody").value());
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::NotFound);
}

TEST_CASE("SqlAccountRepository[sqlite]: save upserts existing",
          "[infra][persistence][sqlite]") {
    SqlAccountRepository repo(make_sqlite_driver());
    REQUIRE(repo.save(make_account(42, "UpdateMe")).has_value());
    REQUIRE_FALSE(
        repo.find_by_name(domain::UserName::parse("UpdateMe").value())
            .value().is_locked());

    auto locked = domain::identity::Account::rehydrate(
        domain::AccountId{42}, domain::UserName::parse("UpdateMe").value(),
        domain::BNHash{}, domain::Locale::parse_or_default("enUS"),
        domain::identity::CommandGroupMask{}, std::nullopt, true, false);
    REQUIRE(repo.save(locked).has_value());
    REQUIRE(repo.find_by_name(domain::UserName::parse("UpdateMe").value())
                .value().is_locked());
}

TEST_CASE("SqlAccountRepository[sqlite]: size / forEach / remove",
          "[infra][persistence][sqlite]") {
    SqlAccountRepository repo(make_sqlite_driver());
    REQUIRE(repo.size() == 0u);
    REQUIRE(repo.save(make_account(1, "Alice")).has_value());
    REQUIRE(repo.save(make_account(2, "Bob")).has_value());
    REQUIRE(repo.save(make_account(3, "Charlie")).has_value());
    REQUIRE(repo.size() == 3u);

    std::size_t seen = 0;
    repo.forEach([&seen](const domain::identity::Account&) { ++seen; return true; });
    REQUIRE(seen == 3u);

    REQUIRE(repo.remove(domain::AccountId{2}).has_value());
    REQUIRE(repo.size() == 2u);
    REQUIRE_FALSE(
        repo.find_by_name(domain::UserName::parse("Bob").value()).has_value());
}

TEST_CASE("RepositoryFactory[sqlite]: create_account_repository over a driver",
          "[infra][persistence][sqlite]") {
    RepositoryFactory factory("sqlite", make_sqlite_driver());
    auto repo = factory.create_account_repository();
    REQUIRE(repo != nullptr);

    REQUIRE(repo->save(make_account(5, "ViaFactory")).has_value());
    auto found = repo->find_by_id(domain::AccountId{5});
    REQUIRE(found.has_value());
    REQUIRE(found.value().name().display() == "ViaFactory");
}

namespace {

domain::chat::Channel make_channel(std::uint32_t id, std::string_view name) {
    domain::chat::ChannelPolicy policy;
    policy.max_members = 100;
    return domain::chat::Channel::rehydrate(
        domain::ChannelId{id}, std::string{name}, "topic", policy, {}, {});
}

}  // namespace

TEST_CASE("SqlChannelRepository[sqlite]: save / find / size / remove",
          "[infra][persistence][sqlite]") {
    SqlChannelRepository repo(make_sqlite_driver());
    REQUIRE(repo.size() == 0u);

    REQUIRE(repo.save(make_channel(1, "Lobby")).has_value());
    REQUIRE(repo.save(make_channel(2, "Help")).has_value());
    REQUIRE(repo.size() == 2u);

    auto by_name = repo.find_by_name("Lobby");
    REQUIRE(by_name.has_value());
    REQUIRE(by_name.value().id().value() == 1u);

    auto by_id = repo.find_by_id(domain::ChannelId{2});
    REQUIRE(by_id.has_value());
    REQUIRE(by_id.value().name() == "Help");

    REQUIRE(repo.remove(domain::ChannelId{1}).has_value());
    REQUIRE(repo.size() == 1u);
    REQUIRE_FALSE(repo.find_by_name("Lobby").has_value());
}

TEST_CASE("RepositoryFactory[sqlite]: create_channel_repository over a driver",
          "[infra][persistence][sqlite]") {
    RepositoryFactory factory("sqlite", make_sqlite_driver());
    auto repo = factory.create_channel_repository();
    REQUIRE(repo != nullptr);
    REQUIRE(repo->save(make_channel(7, "ViaFactory")).has_value());
    REQUIRE(repo->find_by_id(domain::ChannelId{7}).has_value());
}

}  // namespace pvpgn::infra::persistence
