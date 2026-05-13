// SPDX-License-Identifier: GPL-2.0-or-later
#include <variant>

#include <catch2/catch_test_macros.hpp>

#include "domain/realm/realm.hpp"

using namespace pvpgn;
using domain::AccountId;
using domain::realm::Realm;

TEST_CASE("Realm::create rejects bad name and emits RealmRegistered",
          "[domain][realm]") {
    REQUIRE_FALSE(Realm::create(1, "", "desc").has_value());
    auto r = Realm::create(1, "USEast", "Realm East").value();
    auto evs = r.drain_events();
    REQUIRE(evs.size() == 1);
    REQUIRE(std::holds_alternative<domain::events::RealmRegistered>(evs[0]));
    REQUIRE(r.active());
}

TEST_CASE("Realm: create_character / delete_character with case-insensitive lookup",
          "[domain][realm][character]") {
    auto r = Realm::create(1, "USEast", "").value();
    (void)r.drain_events();

    REQUIRE(r.create_character(AccountId{1}, "", "Sorc") == Realm::CreateOutcome::BadName);
    REQUIRE(r.create_character(AccountId{1}, std::string(20, 'x'), "Sorc") ==
            Realm::CreateOutcome::BadName);
    REQUIRE(r.create_character(AccountId{1}, "Hero", "Sorc") ==
            Realm::CreateOutcome::Created);
    REQUIRE(r.character_count() == 1);

    // Case-insensitive duplicate detection.
    REQUIRE(r.create_character(AccountId{2}, "HERO", "Necro") ==
            Realm::CreateOutcome::NameTaken);

    auto evs = r.drain_events();
    REQUIRE(evs.size() == 1);
    REQUIRE(std::holds_alternative<domain::events::CharacterCreated>(evs[0]));

    REQUIRE_FALSE(r.delete_character(AccountId{2}, "Hero"));  // wrong owner
    REQUIRE(r.delete_character(AccountId{1}, "hero"));        // case-insensitive
    REQUIRE(r.character_count() == 0);
}

TEST_CASE("Realm::unregister is one-way and emits RealmUnregistered",
          "[domain][realm]") {
    auto r = Realm::create(1, "USEast", "").value();
    (void)r.drain_events();
    r.unregister();
    REQUIRE_FALSE(r.active());
    auto evs = r.drain_events();
    REQUIRE(evs.size() == 1);
    REQUIRE(std::holds_alternative<domain::events::RealmUnregistered>(evs[0]));
    r.unregister();
    REQUIRE(r.drain_events().empty());
}
