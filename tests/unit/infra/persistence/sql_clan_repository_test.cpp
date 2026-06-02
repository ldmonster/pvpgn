// SPDX-License-Identifier: GPL-2.0-or-later
//
// tests/unit/infra/persistence/sql_clan_repository_test.cpp -- Plan 07.
//
// Verifies the consolidated SqlClanRepository over the recording fake IDbDriver
// (no sqlite). Clan is a parent row + ordered member list, so find_* issues two
// queries (clan header, then members) — exercised via the driver's per-query
// result-set queue. Also pins the transactional save and the tag-scoped remove.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "domain/social/clan.hpp"
#include "infra/persistence/clan_repository.hpp"

#include "recording_fake_driver.hpp"

using namespace pvpgn::infra::persistence;
using namespace pvpgn::test::persistence;
using pvpgn::domain::AccountId;
using pvpgn::domain::ClanId;
using pvpgn::domain::ClientTag;
using pvpgn::domain::social::Clan;
using pvpgn::domain::social::ClanMember;
using pvpgn::domain::social::ClanRank;

namespace {

std::shared_ptr<RecordingFakeDriver> make_driver() {
    return std::make_shared<RecordingFakeDriver>();
}

// clans row: id, tag, name, client_tag
FakeRow clan_row(std::int64_t id, std::string tag, std::string name,
                 std::string client) {
    return FakeRow{std::vector<Cell>{id, std::move(tag), std::move(name),
                                     std::move(client)}};
}

// clan_members row: account_id, rank
FakeRow member_row(std::int64_t account, std::int64_t rank) {
    return FakeRow{std::vector<Cell>{account, rank}};
}

}  // namespace

TEST_CASE("SqlClanRepository::find_by_id loads the clan and its ordered members",
          "[infra][persistence][clan]") {
    auto driver = make_driver();
    SqlClanRepository repo{driver};

    // Query 1 → the clan header; Query 2 → its members.
    driver->push_result_set({clan_row(10, "WoW", "Wolf Pack", "WAR3")});
    driver->push_result_set({
        member_row(1, static_cast<std::int64_t>(ClanRank::Chieftain)),
        member_row(2, static_cast<std::int64_t>(ClanRank::Grunt)),
    });

    auto r = repo.find_by_id(ClanId{10});
    REQUIRE(r.has_value());
    auto clan = r.value();
    REQUIRE(clan != nullptr);
    CHECK(clan->id().value() == 10u);
    CHECK(clan->tag() == "WoW");
    CHECK(clan->name() == "Wolf Pack");
    CHECK(clan->client().text() == "WAR3");

    REQUIRE(clan->members().size() == 2);
    CHECK(clan->members()[0].account.value() == 1u);
    CHECK(clan->members()[0].rank == ClanRank::Chieftain);
    CHECK(clan->members()[1].account.value() == 2u);
    CHECK(clan->members()[1].rank == ClanRank::Grunt);

    // Two queries: clans (bound id) then clan_members (bound clan_id).
    REQUIRE(driver->calls.size() == 2);
    CHECK(driver->calls[0].sql.find("FROM clans WHERE id = ?") != std::string::npos);
    CHECK(as_int(driver->calls[0].params.at(0)) == 10);
    CHECK(driver->calls[1].sql.find("FROM clan_members WHERE clan_id = ?")
          != std::string::npos);
    CHECK(driver->calls[1].sql.find("ORDER BY position") != std::string::npos);
    CHECK(as_int(driver->calls[1].params.at(0)) == 10);
}

TEST_CASE("SqlClanRepository::find_by_id is NotFound when the clan row is absent",
          "[infra][persistence][clan]") {
    auto driver = make_driver();
    SqlClanRepository repo{driver};
    driver->push_result_set({});  // no clan header

    auto r = repo.find_by_id(ClanId{404});
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().code() == pvpgn::core::StatusCode::NotFound);
}

TEST_CASE("SqlClanRepository::find_by_tag / find_by_name bind their key",
          "[infra][persistence][clan]") {
    SECTION("by tag") {
        auto driver = make_driver();
        SqlClanRepository repo{driver};
        driver->push_result_set({clan_row(3, " XYZ", "Team", "STAR")});
        driver->push_result_set({});  // no members

        auto r = repo.find_by_tag("XYZ");
        REQUIRE(r.has_value());
        CHECK(driver->calls[0].sql.find("WHERE tag = ?") != std::string::npos);
        CHECK(as_str(driver->calls[0].params.at(0)) == "XYZ");
    }
    SECTION("by name (case-insensitive)") {
        auto driver = make_driver();
        SqlClanRepository repo{driver};
        driver->push_result_set({clan_row(4, "ABC", "My Clan", "STAR")});
        driver->push_result_set({});

        auto r = repo.find_by_name("my clan");
        REQUIRE(r.has_value());
        CHECK(driver->calls[0].sql.find("name = ? COLLATE NOCASE")
              != std::string::npos);
        CHECK(as_str(driver->calls[0].params.at(0)) == "my clan");
    }
}

TEST_CASE("SqlClanRepository::find rejects an invalid stored client_tag",
          "[infra][persistence][clan]") {
    auto driver = make_driver();
    SqlClanRepository repo{driver};
    driver->push_result_set({clan_row(5, "BAD", "Broken", "TOOLONG")});

    auto r = repo.find_by_id(ClanId{5});
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().code() == pvpgn::core::StatusCode::Internal);
}

TEST_CASE("SqlClanRepository::save upserts the clan and replaces members atomically",
          "[infra][persistence][clan]") {
    auto driver = make_driver();
    SqlClanRepository repo{driver};

    Clan clan = Clan::rehydrate(
        ClanId{10}, "WoW", "Wolf Pack", ClientTag::parse("WAR3").value(),
        {ClanMember{AccountId{1}, ClanRank::Chieftain},
         ClanMember{AccountId{2}, ClanRank::Peon}});

    REQUIRE(repo.save(clan).has_value());

    CHECK(driver->begin_count == 1);
    CHECK(driver->commit_count == 1);
    CHECK(driver->rollback_count == 0);

    // upsert clans, delete members, then two ordered member inserts.
    REQUIRE(driver->calls.size() == 4);
    CHECK(driver->calls[0].sql.find("INSERT OR REPLACE INTO clans")
          != std::string::npos);
    CHECK(as_int(driver->calls[0].params.at(0)) == 10);
    CHECK(as_str(driver->calls[0].params.at(1)) == "WoW");
    CHECK(as_str(driver->calls[0].params.at(3)) == "WAR3");  // client tag text

    CHECK(driver->calls[1].sql.find("DELETE FROM clan_members WHERE clan_id = ?")
          != std::string::npos);

    CHECK(driver->calls[2].sql.find("INSERT INTO clan_members") != std::string::npos);
    CHECK(as_int(driver->calls[2].params.at(1)) == 1);  // account
    CHECK(as_int(driver->calls[2].params.at(2)) ==
          static_cast<std::int64_t>(ClanRank::Chieftain));
    CHECK(as_int(driver->calls[2].params.at(3)) == 0);  // position
    CHECK(as_int(driver->calls[3].params.at(3)) == 1);  // position
}

TEST_CASE("SqlClanRepository::remove deletes members then clan by tag, transactionally",
          "[infra][persistence][clan]") {
    auto driver = make_driver();
    SqlClanRepository repo{driver};

    REQUIRE(repo.remove("WoW").has_value());

    CHECK(driver->begin_count == 1);
    CHECK(driver->commit_count == 1);
    REQUIRE(driver->calls.size() == 2);
    CHECK(driver->calls[0].sql.find("DELETE FROM clan_members WHERE clan_id IN")
          != std::string::npos);
    CHECK(as_str(driver->calls[0].params.at(0)) == "WoW");
    CHECK(driver->calls[1].sql.find("DELETE FROM clans WHERE tag = ?")
          != std::string::npos);
    CHECK(as_str(driver->calls[1].params.at(0)) == "WoW");
}
