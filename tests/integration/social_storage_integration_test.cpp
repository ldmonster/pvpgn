// SPDX-License-Identifier: GPL-2.0-or-later

/// @file social_storage_integration_test.cpp
/// Integration tests for the four SQL repositories whose emitted SQL had
/// drifted from the embedded migration schema: clans, friend lists, ladder, and
/// IP bans. The repo *unit* tests use a fake recording driver that only
/// string-matches the SQL and replays canned rows, so the table/column
/// divergence from `all_migrations.cpp` was invisible there.
///
/// These tests run the *real* embedded migrations into an on-disk SQLite file,
/// build each repository over a `SqliteDriver`, then save an entity and find it
/// back — asserting the fields round-trip against the schema the runtime
/// actually creates. They are the forcing function proving the schema/repo
/// reconciliation.
///
/// SQLite tests always run (on-disk temp files; no external services).

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>

#ifdef PVPGN_HAS_INFRA_SQLITE
#include "domain/ladder/ladder.hpp"
#include "domain/moderation/ip_ban_list.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/ip_address.hpp"
#include "domain/social/clan.hpp"
#include "domain/social/friend_list.hpp"

#include "infra/migrations/all_migrations.hpp"
#include "infra/migrations/migration_runner.hpp"
#include "infra/persistence/clan_repository.hpp"
#include "infra/persistence/friend_list_repository.hpp"
#include "infra/persistence/ip_ban_repository.hpp"
#include "infra/persistence/ladder_repository.hpp"
#include "infra/persistence/sql_builder/sqlite_driver.hpp"
#include "infra/sqlite/connection.hpp"
#endif

namespace pvpgn::integration {

#ifdef PVPGN_HAS_INFRA_SQLITE

namespace {

/// RAII temp directory — created on construction, removed on destruction.
class TempDir {
public:
    TempDir() {
        base_ = std::filesystem::temp_directory_path() /
                ("pvpgn_social_integ_" +
                 std::to_string(std::chrono::steady_clock::now()
                                    .time_since_epoch()
                                    .count()));
        std::filesystem::create_directories(base_);
    }

    ~TempDir() noexcept {
        std::error_code ec;
        std::filesystem::remove_all(base_, ec);
    }

    std::string db_file() const { return (base_ / "social.db").string(); }

private:
    std::filesystem::path base_;
};

/// Open a fresh on-disk SQLite DB with the real embedded schema applied, and
/// return a driver over it. The connection is kept alive by the returned
/// driver's shared_ptr.
std::shared_ptr<infra::persistence::SqliteDriver> migrated_driver(
    const std::string& db_path) {
    auto conn = std::make_shared<infra::sqlite::SQLiteConnection>(db_path);

    infra::migrations::MigrationRunner runner(
        [conn](std::string_view sql) { return conn->exec(sql); },
        [conn]() -> std::optional<std::uint32_t> {
            std::optional<std::uint32_t> version;
            (void)conn->query(
                "SELECT MAX(version) FROM _schema_migrations",
                [&version](const infra::sqlite::Row& row) {
                    if (!row.is_null(0)) {
                        version = static_cast<std::uint32_t>(row.get_int(0));
                    }
                    return false;
                });
            return version;
        });
    REQUIRE(runner.ensure_migration_table().has_value());
    REQUIRE(runner.migrate_to_latest(infra::migrations::get_all_migrations())
                .has_value());

    return std::make_shared<infra::persistence::SqliteDriver>(conn);
}

}  // namespace

// ---------------------------------------------------------------------------
// Clans (F-CLAN-1)
// ---------------------------------------------------------------------------

TEST_CASE("SqlClanRepository over SQLite: save + find round-trips clan and "
          "ordered members against the real schema",
          "[integration][sqlite][clan]") {
    TempDir tmp;
    auto driver = migrated_driver(tmp.db_file());
    infra::persistence::SqlClanRepository repo(driver);

    auto client = domain::ClientTag::parse("WAR3");
    REQUIRE(client.has_value());

    std::vector<domain::social::ClanMember> members{
        {domain::AccountId{1}, domain::social::ClanRank::Chieftain},
        {domain::AccountId{2}, domain::social::ClanRank::Grunt},
    };
    auto clan = domain::social::Clan::rehydrate(
        domain::ClanId{10}, "WoW", "Wolf Pack", client.value(),
        std::move(members));

    REQUIRE(repo.save(clan).has_value());

    auto found = repo.find_by_tag("WoW");
    REQUIRE(found.has_value());
    auto loaded = found.value();
    REQUIRE(loaded != nullptr);
    CHECK(loaded->id().value() == 10u);
    CHECK(loaded->tag() == "WoW");
    CHECK(loaded->name() == "Wolf Pack");
    CHECK(loaded->client().text() == "WAR3");

    REQUIRE(loaded->members().size() == 2);
    // Members come back in stored `position` order.
    CHECK(loaded->members()[0].account.value() == 1u);
    CHECK(loaded->members()[0].rank == domain::social::ClanRank::Chieftain);
    CHECK(loaded->members()[1].account.value() == 2u);
    CHECK(loaded->members()[1].rank == domain::social::ClanRank::Grunt);

    // find_by_id and find_by_name resolve the same row.
    auto by_id = repo.find_by_id(domain::ClanId{10});
    REQUIRE(by_id.has_value());
    CHECK(by_id.value()->tag() == "WoW");
}

// ---------------------------------------------------------------------------
// Friend lists (F-FRIEND-1)
// ---------------------------------------------------------------------------

TEST_CASE("SqlFriendListRepository over SQLite: save + find preserves order "
          "against the real schema",
          "[integration][sqlite][friend]") {
    TempDir tmp;
    auto driver = migrated_driver(tmp.db_file());
    infra::persistence::SqlFriendListRepository repo(driver);

    auto list = domain::social::FriendList::rehydrate(
        domain::AccountId{100},
        {domain::AccountId{7}, domain::AccountId{3}, domain::AccountId{9}});

    REQUIRE(repo.save(list).has_value());

    auto found = repo.find_by_owner(domain::AccountId{100});
    REQUIRE(found.has_value());
    REQUIRE(found->entries().size() == 3);
    // The `position` column preserves the original insertion order.
    CHECK(found->entries()[0].value() == 7u);
    CHECK(found->entries()[1].value() == 3u);
    CHECK(found->entries()[2].value() == 9u);

    // An owner with no friends is a valid empty list, not an error.
    auto empty = repo.find_by_owner(domain::AccountId{999});
    REQUIRE(empty.has_value());
    CHECK(empty->entries().empty());

    // Re-saving replaces the whole list in order.
    auto shorter = domain::social::FriendList::rehydrate(
        domain::AccountId{100}, {domain::AccountId{42}});
    REQUIRE(repo.save(shorter).has_value());
    auto reloaded = repo.find_by_owner(domain::AccountId{100});
    REQUIRE(reloaded.has_value());
    REQUIRE(reloaded->entries().size() == 1);
    CHECK(reloaded->entries()[0].value() == 42u);
}

// ---------------------------------------------------------------------------
// Ladder (F-LADDER-1)
// ---------------------------------------------------------------------------

TEST_CASE("SqlLadderRepository over SQLite: save_entry + get_top_n + get_rank "
          "round-trip against the real schema",
          "[integration][sqlite][ladder]") {
    TempDir tmp;
    auto driver = migrated_driver(tmp.db_file());
    infra::persistence::SqlLadderRepository repo(driver);

    domain::ladder::LadderEntry top;
    top.account     = domain::AccountId{1};
    top.rating      = 2000;
    top.wins        = 20;
    top.losses      = 1;
    top.disconnects = 0;

    domain::ladder::LadderEntry mid;
    mid.account     = domain::AccountId{2};
    mid.rating      = 1800;
    mid.wins        = 15;
    mid.losses      = 5;
    mid.disconnects = 2;

    REQUIRE(repo.save_entry(top).has_value());
    REQUIRE(repo.save_entry(mid).has_value());

    auto leaders = repo.get_top_n(10);
    REQUIRE(leaders.has_value());
    REQUIRE(leaders->size() == 2);
    // Ordered by rating DESC; every field survives the round-trip.
    CHECK((*leaders)[0].account.value() == 1u);
    CHECK((*leaders)[0].rating == 2000);
    CHECK((*leaders)[0].wins == 20u);
    CHECK((*leaders)[0].losses == 1u);
    CHECK((*leaders)[0].disconnects == 0u);
    CHECK((*leaders)[1].account.value() == 2u);
    CHECK((*leaders)[1].rating == 1800);
    CHECK((*leaders)[1].disconnects == 2u);

    // Rank = 1 + (#strictly-higher).
    auto rank_top = repo.get_rank(domain::AccountId{1});
    REQUIRE(rank_top.has_value());
    CHECK(rank_top.value() == 1u);
    auto rank_mid = repo.get_rank(domain::AccountId{2});
    REQUIRE(rank_mid.has_value());
    CHECK(rank_mid.value() == 2u);

    // save_entry is an upsert keyed by account_id.
    top.rating = 2500;
    REQUIRE(repo.save_entry(top).has_value());
    auto after = repo.get_top_n(10);
    REQUIRE(after.has_value());
    REQUIRE(after->size() == 2);  // still two rows, not three
    CHECK((*after)[0].rating == 2500);
}

// ---------------------------------------------------------------------------
// IP bans (F-IPBAN-1)
// ---------------------------------------------------------------------------

TEST_CASE("SqlIpBanRepository over SQLite: exact + range bans round-trip "
          "against the real schema",
          "[integration][sqlite][ip_ban]") {
    TempDir tmp;
    auto driver = migrated_driver(tmp.db_file());
    infra::persistence::SqlIpBanRepository repo(driver);

    auto ip = domain::IpAddress::parse("203.0.113.7");
    REQUIRE(ip.has_value());

    domain::moderation::IpBanEntry entry;
    entry.ip         = ip.value();
    entry.reason     = "integration-test";
    entry.issuer     = domain::AccountId{1};
    entry.issued_at  = core::SystemTime{} + std::chrono::seconds{1000};
    entry.expires_at = std::nullopt;  // never expires

    // Exact-host ban round-trips through the `ip_bans` table.
    REQUIRE(repo.add_ban(entry).has_value());

    auto banned = repo.is_banned(ip.value());
    REQUIRE(banned.has_value());
    CHECK(banned.value());

    auto other = domain::IpAddress::parse("198.51.100.1");
    REQUIRE(other.has_value());
    auto not_banned = repo.is_banned(other.value());
    REQUIRE(not_banned.has_value());
    CHECK_FALSE(not_banned.value());

    // The banlist aggregate reloads the exact-host entries with their fields.
    auto banlist = repo.load_banlist();
    REQUIRE(banlist.has_value());
    REQUIRE(banlist->entries().size() == 1);
    CHECK(banlist->entries()[0].reason == "integration-test");
    CHECK(banlist->entries()[0].issuer.value() == 1u);

    // CIDR range ban round-trips through the `ip_ban_ranges` table; an address
    // inside the range is banned, one outside is not.
    auto net = domain::IpAddress::parse("10.0.0.0");
    REQUIRE(net.has_value());
    REQUIRE(repo.add_range_ban(net.value(), 8, "range-test",
                               domain::AccountId{1},
                               core::SystemTime{} + std::chrono::seconds{1000},
                               std::nullopt)
                .has_value());

    auto inside = domain::IpAddress::parse("10.5.6.7");
    REQUIRE(inside.has_value());
    auto inside_banned = repo.is_banned(inside.value());
    REQUIRE(inside_banned.has_value());
    CHECK(inside_banned.value());

    // Remove the exact ban; the host is no longer matched.
    REQUIRE(repo.remove_ban(ip.value()).has_value());
    auto after_remove = repo.is_banned(ip.value());
    REQUIRE(after_remove.has_value());
    CHECK_FALSE(after_remove.value());

    // Remove the range ban; the previously-inside address is freed.
    REQUIRE(repo.remove_range_ban(net.value(), 8).has_value());
    auto inside_after = repo.is_banned(inside.value());
    REQUIRE(inside_after.has_value());
    CHECK_FALSE(inside_after.value());
}

#endif  // PVPGN_HAS_INFRA_SQLITE

}  // namespace pvpgn::integration
