#include <catch2/catch_test_macros.hpp>
#include "domain/realm/character.hpp"
#include "core/clock.hpp"
#include <chrono>
#include <cstdint>

namespace pvpgn::domain::realm {

// Fixed epoch used across all tests for deterministic time.
static const core::SystemTime kEpoch{};
static const core::SystemTime kEpochPlus1s =
    kEpoch + std::chrono::seconds{1};

TEST_CASE("Character - construction", "[domain][realm]") {
    CharacterId id{"player1", "Barbarian"};
    CharacterStats stats;
    stats.level = 10;
    stats.char_class = CharacterClass::barbarian;
    stats.expansion = CharacterExpansion::lod;

    Character character(id, stats, kEpoch);

    CHECK(character.id().account_name == "player1");
    CHECK(character.id().char_name == "Barbarian");
    CHECK(character.stats().level == 10);
    CHECK(character.stats().char_class == CharacterClass::barbarian);
    CHECK(character.stats().expansion == CharacterExpansion::lod);
}

TEST_CASE("Character - initially not locked", "[domain][realm]") {
    CharacterId id{"player1", "Sorceress"};
    CharacterStats stats;
    Character character(id, stats, kEpoch);

    CHECK_FALSE(character.is_locked());
    CHECK_FALSE(character.locked_by().has_value());
}

TEST_CASE("Character - lock character", "[domain][realm]") {
    CharacterId id{"player1", "Paladin"};
    CharacterStats stats;
    Character character(id, stats, kEpoch);

    auto result = character.lock("gs1.example.com");

    REQUIRE(result);
    CHECK(character.is_locked());
    CHECK(character.locked_by().value() == "gs1.example.com");
}

TEST_CASE("Character - lock already locked character fails", "[domain][realm]") {
    CharacterId id{"player1", "Amazon"};
    CharacterStats stats;
    Character character(id, stats, kEpoch);

    auto lock1 = character.lock("gs1.example.com");
    REQUIRE(lock1);

    auto lock2 = character.lock("gs2.example.com");
    CHECK_FALSE(lock2);

    // Should still be locked by first GS
    CHECK(character.locked_by().value() == "gs1.example.com");
}

TEST_CASE("Character - unlock character", "[domain][realm]") {
    CharacterId id{"player1", "Necromancer"};
    CharacterStats stats;
    Character character(id, stats, kEpoch);

    auto lock_result = character.lock("gs1.example.com");
    REQUIRE(lock_result);
    REQUIRE(character.is_locked());

    auto result = character.unlock("gs1.example.com");

    REQUIRE(result);
    CHECK_FALSE(character.is_locked());
    CHECK_FALSE(character.locked_by().has_value());
}

TEST_CASE("Character - unlock unlocked character fails", "[domain][realm]") {
    CharacterId id{"player1", "Druid"};
    CharacterStats stats;
    Character character(id, stats, kEpoch);

    auto result = character.unlock("gs1.example.com");

    CHECK_FALSE(result);
}

TEST_CASE("Character - unlock with wrong GS fails", "[domain][realm]") {
    CharacterId id{"player1", "Assassin"};
    CharacterStats stats;
    Character character(id, stats, kEpoch);

    auto lock_result = character.lock("gs1.example.com");
    REQUIRE(lock_result);

    auto result = character.unlock("gs2.example.com");

    CHECK_FALSE(result);
    // Should still be locked
    CHECK(character.is_locked());
    CHECK(character.locked_by().value() == "gs1.example.com");
}

TEST_CASE("Character - touch updates last_played", "[domain][realm]") {
    CharacterId id{"player1", "Barbarian"};
    CharacterStats stats;
    Character character(id, stats, kEpoch);

    auto before = character.last_played();

    // Inject a later time — no sleep needed, fully deterministic.
    character.touch(kEpochPlus1s);

    auto after = character.last_played();

    CHECK(after > before);
}

TEST_CASE("Character - created_at is set", "[domain][realm]") {
    CharacterId id{"player1", "Sorceress"};
    CharacterStats stats;
    Character character(id, stats, kEpoch);

    CHECK(character.created_at() == kEpoch);
}

TEST_CASE("Character - stats are preserved", "[domain][realm]") {
    CharacterId id{"player1", "Paladin"};
    CharacterStats stats;
    stats.level = 99;
    stats.experience = 1000000;
    stats.char_class = CharacterClass::paladin;
    stats.expansion = CharacterExpansion::lod;
    stats.hardcore = CharacterHardcore::hardcore;
    stats.dead = false;
    stats.strength = 100;
    stats.dexterity = 50;
    stats.vitality = 150;
    stats.energy = 75;

    Character character(id, stats, kEpoch);

    CHECK(character.stats().level == 99);
    CHECK(character.stats().experience == 1000000);
    CHECK(character.stats().char_class == CharacterClass::paladin);
    CHECK(character.stats().expansion == CharacterExpansion::lod);
    CHECK(character.stats().hardcore == CharacterHardcore::hardcore);
    CHECK(character.stats().dead == false);
    CHECK(character.stats().strength == 100);
    CHECK(character.stats().dexterity == 50);
    CHECK(character.stats().vitality == 150);
    CHECK(character.stats().energy == 75);
}

TEST_CASE("Character - lock and unlock cycle", "[domain][realm]") {
    CharacterId id{"player1", "Amazon"};
    CharacterStats stats;
    Character character(id, stats, kEpoch);

    // Lock
    auto lock_result = character.lock("gs1.example.com");
    REQUIRE(lock_result);
    CHECK(character.is_locked());

    // Unlock
    auto unlock_result = character.unlock("gs1.example.com");
    REQUIRE(unlock_result);
    CHECK_FALSE(character.is_locked());

    // Lock again
    auto lock_result2 = character.lock("gs2.example.com");
    REQUIRE(lock_result2);
    CHECK(character.is_locked());
    CHECK(character.locked_by().value() == "gs2.example.com");
}

// Pins the shared-kernel CharacterClass enum to the canonical D2 class ids
// (fixed by the wire/save format). If anyone reorders the enum, this fails.
TEST_CASE("CharacterClass - canonical D2 class id ordering", "[domain][realm]") {
    CHECK(static_cast<std::uint8_t>(CharacterClass::amazon)      == 0);
    CHECK(static_cast<std::uint8_t>(CharacterClass::sorceress)   == 1);
    CHECK(static_cast<std::uint8_t>(CharacterClass::necromancer) == 2);
    CHECK(static_cast<std::uint8_t>(CharacterClass::paladin)     == 3);
    CHECK(static_cast<std::uint8_t>(CharacterClass::barbarian)   == 4);
    CHECK(static_cast<std::uint8_t>(CharacterClass::druid)       == 5);
    CHECK(static_cast<std::uint8_t>(CharacterClass::assassin)    == 6);
}

TEST_CASE("Character - different character classes", "[domain][realm]") {
    std::vector<CharacterClass> classes = {
        CharacterClass::amazon,
        CharacterClass::necromancer,
        CharacterClass::paladin,
        CharacterClass::barbarian,
        CharacterClass::sorceress,
        CharacterClass::druid,
        CharacterClass::assassin
    };

    for (auto char_class : classes) {
        CharacterId id{"player1", "TestChar"};
        CharacterStats stats;
        stats.char_class = char_class;
        Character character(id, stats, kEpoch);

        CHECK(character.stats().char_class == char_class);
    }
}

} // namespace pvpgn::domain::realm
