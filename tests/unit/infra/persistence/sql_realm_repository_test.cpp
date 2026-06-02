// SPDX-License-Identifier: GPL-2.0-or-later
//
// tests/unit/infra/persistence/sql_realm_repository_test.cpp -- Plan 07.
//
// Verifies the consolidated SqlRealmRepository over the recording fake
// IDbDriver (no sqlite needed). Pins SQL generation, parameter binding, the
// COUNT path, and the row → Realm rehydration (including the active flag).

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "domain/realm/realm.hpp"
#include "infra/persistence/realm_repository.hpp"

#include "recording_fake_driver.hpp"

using namespace pvpgn::infra::persistence;
using namespace pvpgn::test::persistence;
using pvpgn::domain::realm::Realm;

namespace {

std::shared_ptr<RecordingFakeDriver> make_driver() {
    return std::make_shared<RecordingFakeDriver>();
}

// Build a realm row: id, name, description, active(0/1).
FakeRow realm_row(std::int64_t id, std::string name, std::string desc,
                  std::int64_t active) {
    return FakeRow{std::vector<Cell>{id, std::move(name), std::move(desc),
                                     active}};
}

}  // namespace

TEST_CASE("SqlRealmRepository::save issues the bound upsert",
          "[infra][persistence][realm]") {
    auto driver = make_driver();
    SqlRealmRepository repo{driver};

    auto created = Realm::create(7, "Azeroth", "the main realm");
    REQUIRE(created.has_value());
    REQUIRE(repo.save(created.value()).has_value());

    REQUIRE(driver->calls.size() == 1);
    const auto& call = driver->last();
    CHECK(call.sql.find("INSERT OR REPLACE INTO realms") != std::string::npos);
    REQUIRE(call.params.size() == 4);
    CHECK(as_int(call.params[0]) == 7);            // id
    CHECK(as_str(call.params[1]) == "Azeroth");    // name
    CHECK(as_str(call.params[2]) == "the main realm");
    CHECK(as_int(call.params[3]) == 1);            // active (create() → active)
}

TEST_CASE("SqlRealmRepository::find_by_id rehydrates an active realm",
          "[infra][persistence][realm]") {
    auto driver = make_driver();
    SqlRealmRepository repo{driver};
    driver->next_rows.push_back(realm_row(7, "Azeroth", "desc", 1));

    auto r = repo.find_by_id(7);
    REQUIRE(r.has_value());
    CHECK(r.value().id() == 7u);
    CHECK(r.value().name() == "Azeroth");
    CHECK(r.value().description() == "desc");
    CHECK(r.value().active());

    const auto& call = driver->last();
    CHECK(call.sql.find("WHERE id = ?") != std::string::npos);
    REQUIRE(call.params.size() == 1);
    CHECK(as_int(call.params[0]) == 7);
}

TEST_CASE("SqlRealmRepository::find_by_id reflects a stored inactive flag",
          "[infra][persistence][realm]") {
    auto driver = make_driver();
    SqlRealmRepository repo{driver};
    driver->next_rows.push_back(realm_row(9, "Retired", "", 0));  // inactive

    auto r = repo.find_by_id(9);
    REQUIRE(r.has_value());
    CHECK_FALSE(r.value().active());
    // Rehydration events (RealmRegistered/RealmUnregistered) were drained.
}

TEST_CASE("SqlRealmRepository::find_by_id returns NotFound when no row",
          "[infra][persistence][realm]") {
    auto driver = make_driver();  // next_rows empty
    SqlRealmRepository repo{driver};

    auto r = repo.find_by_id(123);
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().code() == pvpgn::core::StatusCode::NotFound);
}

TEST_CASE("SqlRealmRepository::find_by_name binds the name (case-insensitive)",
          "[infra][persistence][realm]") {
    auto driver = make_driver();
    SqlRealmRepository repo{driver};
    driver->next_rows.push_back(realm_row(3, "Lordaeron", "d", 1));

    auto r = repo.find_by_name("lordaeron");
    REQUIRE(r.has_value());
    CHECK(r.value().id() == 3u);

    const auto& call = driver->last();
    CHECK(call.sql.find("name = ? COLLATE NOCASE") != std::string::npos);
    REQUIRE(call.params.size() == 1);
    CHECK(as_str(call.params[0]) == "lordaeron");
}

TEST_CASE("SqlRealmRepository::remove issues a bound DELETE",
          "[infra][persistence][realm]") {
    auto driver = make_driver();
    SqlRealmRepository repo{driver};

    REQUIRE(repo.remove(7).has_value());
    const auto& call = driver->last();
    CHECK(call.sql.find("DELETE FROM realms WHERE id = ?") != std::string::npos);
    REQUIRE(call.params.size() == 1);
    CHECK(as_int(call.params[0]) == 7);
}

TEST_CASE("SqlRealmRepository::size runs COUNT and returns it",
          "[infra][persistence][realm]") {
    auto driver = make_driver();
    SqlRealmRepository repo{driver};
    driver->next_rows.push_back(FakeRow{std::vector<Cell>{std::int64_t{5}}});

    CHECK(repo.size() == 5u);
    CHECK(driver->last().sql.find("SELECT COUNT(*) FROM realms")
          != std::string::npos);
}

TEST_CASE("SqlRealmRepository::forEach maps rows and honours early stop",
          "[infra][persistence][realm]") {
    auto driver = make_driver();
    SqlRealmRepository repo{driver};
    driver->next_rows.push_back(realm_row(1, "A", "", 1));
    driver->next_rows.push_back(realm_row(2, "B", "", 0));

    std::vector<std::uint32_t> ids;
    repo.forEach([&](const Realm& r) {
        ids.push_back(r.id());
        return true;
    });
    REQUIRE(ids.size() == 2);
    CHECK(ids[0] == 1u);
    CHECK(ids[1] == 2u);

    SECTION("predicate returning false stops after the first realm") {
        std::vector<std::uint32_t> ids2;
        repo.forEach([&](const Realm& r) {
            ids2.push_back(r.id());
            return false;
        });
        CHECK(ids2.size() == 1);
    }
}
