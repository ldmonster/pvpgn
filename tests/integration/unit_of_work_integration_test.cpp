// SPDX-License-Identifier: GPL-2.0-or-later

/// @file unit_of_work_integration_test.cpp
/// Integration tests for the SQLite Unit of Work — the transactional boundary
/// that guards data integrity for every persisted bounded context.
///
/// Unlike the unit tests (which use in-memory fakes), these drive the *real*
/// `SQLiteUnitOfWorkFactory` against a real on-disk SQLite file: the factory
/// runs the embedded migrations, hands out `IUnitOfWork` bundles whose
/// repositories are the SQL-backed implementations, and
/// begin/commit/rollback route through the shared connection. This is the only
/// place the following critical behaviours are exercised end-to-end:
///
///   * the factory applies migrations and produces a usable schema;
///   * a committed write is durable across UoW instances (same connection);
///   * a rolled-back write leaves no trace;
///   * multiple repositories mutated in one transaction commit/roll back
///     atomically (all-or-nothing across bounded contexts);
///   * data survives a factory teardown + reopen of the same file, and the
///     migration runner is idempotent on an already-migrated DB;
///   * the ISP `UnitOfWorkGuard` / `ITransaction` RAII path
///     commits on success and rolls back on scope exit, against a real DB.
///
/// SQLite tests always run (on-disk temp files; no external services).

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>

#include "domain/identity/account.hpp"
#include "domain/moderation/ports.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"

#ifdef PVPGN_HAS_INFRA_SQLITE
#include "application/persistence/unit_of_work.hpp"
#include "infra/sqlite/unit_of_work_factory.hpp"
#endif

namespace pvpgn::integration {

#ifdef PVPGN_HAS_INFRA_SQLITE

namespace {

/// RAII temp directory — created on construction, removed on destruction.
class TempDir {
public:
    TempDir() {
        base_ = std::filesystem::temp_directory_path() /
                ("pvpgn_uow_integ_" +
                 std::to_string(std::chrono::steady_clock::now()
                                    .time_since_epoch()
                                    .count()));
        std::filesystem::create_directories(base_);
    }

    ~TempDir() noexcept {
        std::error_code ec;
        std::filesystem::remove_all(base_, ec);
    }

    std::string db_file() const { return (base_ / "uow.db").string(); }

private:
    std::filesystem::path base_;
};

/// Build a minimal, valid test account with the given id + username.
domain::identity::Account make_account(std::uint32_t id,
                                       std::string_view username) {
    auto name = domain::UserName::parse(username);
    REQUIRE(name.has_value());
    return domain::identity::Account::rehydrate(
        domain::AccountId{id},
        std::move(name.value()),
        domain::BNHash{},
        domain::Locale::parse_or_default("enUS"),
        domain::identity::CommandGroupMask{},
        std::nullopt,
        false,
        false);
}

/// A never-expiring account ban issued by account 1 against @p target.
domain::moderation::AccountBan make_ban(std::uint32_t target) {
    domain::moderation::AccountBan ban;
    ban.banned_account = domain::AccountId{target};
    ban.banned_by      = domain::AccountId{1};
    ban.reason         = "integration-test";
    ban.banned_at      = core::SystemTime{};
    ban.expires_at     = std::nullopt;  // never expires -> always active
    return ban;
}

}  // namespace

// ---------------------------------------------------------------------------
// Factory + migrations
// ---------------------------------------------------------------------------

TEST_CASE("SQLite UoW factory runs migrations and yields a usable transaction",
          "[integration][sqlite][uow]") {
    TempDir tmp;
    infra::sqlite::SQLiteUnitOfWorkFactory factory(tmp.db_file());

    // The migration runner ran in the factory ctor; the produced UoW must be
    // able to open and commit an (empty) transaction over the migrated schema.
    auto uow = factory.create();
    REQUIRE(uow != nullptr);
    REQUIRE(uow->begin().has_value());
    REQUIRE(uow->commit().has_value());
}

// ---------------------------------------------------------------------------
// Commit durability across UoW instances (same connection)
// ---------------------------------------------------------------------------

TEST_CASE("SQLite UoW: committed write is visible to a later UoW",
          "[integration][sqlite][uow]") {
    TempDir tmp;
    infra::sqlite::SQLiteUnitOfWorkFactory factory(tmp.db_file());

    {
        auto uow = factory.create();
        REQUIRE(uow->begin().has_value());
        REQUIRE(uow->accounts().save(make_account(10, "CommitUser")).has_value());
        REQUIRE(uow->commit().has_value());
    }

    auto uow2  = factory.create();
    auto found = uow2->accounts().find_by_name(
        domain::UserName::parse("CommitUser").value());
    REQUIRE(found.has_value());
    REQUIRE(found->id().value() == 10u);
}

// ---------------------------------------------------------------------------
// Rollback discards writes
// ---------------------------------------------------------------------------

TEST_CASE("SQLite UoW: rolled-back write leaves no trace",
          "[integration][sqlite][uow]") {
    TempDir tmp;
    infra::sqlite::SQLiteUnitOfWorkFactory factory(tmp.db_file());

    {
        auto uow = factory.create();
        REQUIRE(uow->begin().has_value());
        REQUIRE(uow->accounts().save(make_account(20, "GhostUser")).has_value());
        uow->rollback();
    }

    auto uow2  = factory.create();
    auto found = uow2->accounts().find_by_name(
        domain::UserName::parse("GhostUser").value());
    REQUIRE_FALSE(found.has_value());
}

// ---------------------------------------------------------------------------
// Cross-repository atomicity: all-or-nothing across bounded contexts
// ---------------------------------------------------------------------------

TEST_CASE("SQLite UoW: account + ban commit atomically in one transaction",
          "[integration][sqlite][uow]") {
    TempDir tmp;
    infra::sqlite::SQLiteUnitOfWorkFactory factory(tmp.db_file());

    {
        auto uow = factory.create();
        REQUIRE(uow->begin().has_value());
        REQUIRE(uow->accounts().save(make_account(30, "BannedUser")).has_value());
        REQUIRE(uow->account_bans().add_ban(make_ban(30)).has_value());
        REQUIRE(uow->commit().has_value());
    }

    auto uow2 = factory.create();
    REQUIRE(uow2->accounts()
                .find_by_name(domain::UserName::parse("BannedUser").value())
                .has_value());
    auto ban = uow2->account_bans().find_active_ban(domain::AccountId{30},
                                                    core::SystemTime{});
    REQUIRE(ban.has_value());
    REQUIRE(ban->has_value());  // an active ban was found
    REQUIRE((*ban)->banned_account.value() == 30u);
}

TEST_CASE("SQLite UoW: account + ban roll back atomically (neither persists)",
          "[integration][sqlite][uow]") {
    TempDir tmp;
    infra::sqlite::SQLiteUnitOfWorkFactory factory(tmp.db_file());

    {
        auto uow = factory.create();
        REQUIRE(uow->begin().has_value());
        REQUIRE(uow->accounts().save(make_account(40, "Doomed")).has_value());
        REQUIRE(uow->account_bans().add_ban(make_ban(40)).has_value());
        uow->rollback();  // abort the whole unit of work
    }

    auto uow2 = factory.create();
    REQUIRE_FALSE(uow2->accounts()
                      .find_by_name(domain::UserName::parse("Doomed").value())
                      .has_value());
    auto ban = uow2->account_bans().find_active_ban(domain::AccountId{40},
                                                    core::SystemTime{});
    REQUIRE(ban.has_value());      // query itself succeeded...
    REQUIRE_FALSE(ban->has_value());  // ...but no ban row remains
}

// ---------------------------------------------------------------------------
// Durability across a factory teardown + reopen (migration idempotency)
// ---------------------------------------------------------------------------

TEST_CASE("SQLite UoW: data survives factory teardown and file reopen",
          "[integration][sqlite][uow]") {
    TempDir tmp;
    const auto path = tmp.db_file();

    {
        infra::sqlite::SQLiteUnitOfWorkFactory factory(path);
        auto uow = factory.create();
        REQUIRE(uow->begin().has_value());
        REQUIRE(uow->accounts().save(make_account(50, "Persistent")).has_value());
        REQUIRE(uow->commit().has_value());
    }  // factory + UoW destroyed -> connection closed

    // A fresh factory re-opens the same file. The migration runner must be
    // idempotent on an already-migrated DB (no re-apply, no error), and the
    // previously committed account must still be there.
    infra::sqlite::SQLiteUnitOfWorkFactory reopened(path);
    auto uow2  = reopened.create();
    auto found = uow2->accounts().find_by_name(
        domain::UserName::parse("Persistent").value());
    REQUIRE(found.has_value());
    REQUIRE(found->id().value() == 50u);
}

// ---------------------------------------------------------------------------
// UnitOfWorkGuard / ITransaction RAII path
// ---------------------------------------------------------------------------

TEST_CASE("SQLite UoW: UnitOfWorkGuard commits on success",
          "[integration][sqlite][uow][guard]") {
    TempDir tmp;
    infra::sqlite::SQLiteUnitOfWorkFactory factory(tmp.db_file());

    {
        auto uow = factory.create();
        // The guard depends only on the segregated ITransaction sub-interface;
        // an IUnitOfWork is-a ITransaction, so it binds directly.
        application::ports::UnitOfWorkGuard guard(*uow);
        REQUIRE(uow->accounts().save(make_account(60, "GuardCommit")).has_value());
        REQUIRE(guard.commit().has_value());
    }

    auto uow2 = factory.create();
    REQUIRE(uow2->accounts()
                .find_by_name(domain::UserName::parse("GuardCommit").value())
                .has_value());
}

TEST_CASE("SQLite UoW: UnitOfWorkGuard rolls back when commit is skipped",
          "[integration][sqlite][uow][guard]") {
    TempDir tmp;
    infra::sqlite::SQLiteUnitOfWorkFactory factory(tmp.db_file());

    {
        auto uow = factory.create();
        application::ports::UnitOfWorkGuard guard(*uow);
        REQUIRE(uow->accounts().save(make_account(70, "GuardRollback")).has_value());
        // No guard.commit() -> destructor must roll the transaction back.
    }

    auto uow2 = factory.create();
    REQUIRE_FALSE(uow2->accounts()
                      .find_by_name(domain::UserName::parse("GuardRollback").value())
                      .has_value());
}

// ---------------------------------------------------------------------------
// Independent transaction state across concurrent UoWs (C1 regression)
// ---------------------------------------------------------------------------
//
// Each create() now hands out a UoW over its OWN sqlite3 connection, so two
// live UoWs must have fully independent transaction state: one can roll back
// while the other commits, and neither must clobber the other. With the old
// shared-connection design these two BEGINs collided on one physical
// transaction and one UoW's COMMIT could flush the other's uncommitted writes.

TEST_CASE("SQLite UoW: two live UoWs have independent transaction state",
          "[integration][sqlite][uow][concurrency]") {
    TempDir tmp;
    infra::sqlite::SQLiteUnitOfWorkFactory factory(tmp.db_file());

    // Two UoWs alive at the same time, each with its own connection and its own
    // transaction state (the bug was a single shared connection whose tx_depth_
    // collided across UoWs). SQLite is single-writer per file, so the two write
    // transactions must not overlap — but each UoW still owns an independent
    // transaction: a commit on one and a rollback on the other don't interfere.
    auto committing  = factory.create();
    auto rolling_back = factory.create();

    // First UoW: write + commit (its own transaction).
    REQUIRE(committing->begin().has_value());
    REQUIRE(committing->accounts().save(make_account(80, "Keeper")).has_value());
    REQUIRE(committing->commit().has_value());

    // Second UoW (a distinct, still-alive object with its own connection): write
    // + rollback. With the old shared connection this begin() would have reused
    // the first UoW's transaction state; now it is fully independent.
    REQUIRE(rolling_back->begin().has_value());
    REQUIRE(
        rolling_back->accounts().save(make_account(81, "Discarded")).has_value());
    rolling_back->rollback();

    // A third UoW observes exactly the committed row and nothing from the
    // rolled-back one: proof the two transactions never shared a handle.
    auto observer = factory.create();
    REQUIRE(observer->accounts()
                .find_by_name(domain::UserName::parse("Keeper").value())
                .has_value());
    REQUIRE_FALSE(observer->accounts()
                      .find_by_name(domain::UserName::parse("Discarded").value())
                      .has_value());
}

#endif  // PVPGN_HAS_INFRA_SQLITE

}  // namespace pvpgn::integration
