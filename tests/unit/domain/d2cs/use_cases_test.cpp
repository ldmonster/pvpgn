// SPDX-License-Identifier: GPL-2.0-or-later
/// @file use_cases_test.cpp
/// Unit tests for domain::d2cs use cases and in-memory repositories.
///
/// Test count: 25 TEST_CASEs, 90+ CHECK/REQUIRE assertions.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "domain/d2cs/in_memory_repositories.hpp"
#include "domain/d2cs/types.hpp"
#include "domain/d2cs/use_cases.hpp"

using namespace pvpgn::domain::d2cs;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

/// Build a minimal valid CharacterInfo.
CharacterInfo make_char(std::string name,
                        CharacterClass cls   = CharacterClass::Sorceress,
                        uint8_t        level = 10,
                        uint32_t       xp    = 1000) {
    CharacterInfo c;
    c.name       = std::move(name);
    c.class_     = cls;
    c.level      = level;
    c.experience = xp;
    c.flags      = CharacterFlags::None;
    c.last_played = 0;
    return c;
}

/// Build a LadderEntry.
LadderEntry make_ladder_entry(std::string char_name,
                              std::string account_name,
                              uint32_t    xp,
                              CharacterClass cls = CharacterClass::Barbarian) {
    LadderEntry e;
    e.character_name = std::move(char_name);
    e.account_name   = std::move(account_name);
    e.class_         = cls;
    e.level          = 50;
    e.experience     = xp;
    e.rank           = 0;  // will be set by repository
    return e;
}

/// Seed a character into the repo, asserting success.
void seed(InMemoryCharacterRepository& repo,
          std::string_view account,
          CharacterInfo info) {
    REQUIRE(repo.save_character(account, info));
}

} // namespace

// ===========================================================================
// CharacterClass enum ordering
// ===========================================================================

// The d2cs session handler casts the raw CREATECHARREQ/CHARLOGINREQ class byte
// straight into CharacterClass, so the ordinals must equal the canonical D2
// class ids. This pins them so a reorder can't silently mislabel classes.
TEST_CASE("CharacterClass — canonical D2 class id ordering", "[d2cs][types]") {
    CHECK(static_cast<uint8_t>(CharacterClass::Amazon)      == 0);
    CHECK(static_cast<uint8_t>(CharacterClass::Sorceress)   == 1);
    CHECK(static_cast<uint8_t>(CharacterClass::Necromancer) == 2);
    CHECK(static_cast<uint8_t>(CharacterClass::Paladin)     == 3);
    CHECK(static_cast<uint8_t>(CharacterClass::Barbarian)   == 4);
    CHECK(static_cast<uint8_t>(CharacterClass::Druid)       == 5);
    CHECK(static_cast<uint8_t>(CharacterClass::Assassin)    == 6);
}

// ===========================================================================
// CharacterListUseCase
// ===========================================================================

TEST_CASE("CharacterListUseCase — empty repository returns empty list", "[d2cs][char_list]") {
    InMemoryCharacterRepository repo;
    CharacterListUseCase uc{repo};

    auto result = uc.execute("alice");
    REQUIRE(result.has_value());
    CHECK(result->empty());
}

TEST_CASE("CharacterListUseCase — single character", "[d2cs][char_list]") {
    InMemoryCharacterRepository repo;
    seed(repo, "alice", make_char("Sorc1", CharacterClass::Sorceress, 20, 5000));

    CharacterListUseCase uc{repo};
    auto result = uc.execute("alice");

    REQUIRE(result.has_value());
    REQUIRE(result->size() == 1);
    CHECK(result->at(0).name == "Sorc1");
    CHECK(result->at(0).class_ == CharacterClass::Sorceress);
    CHECK(result->at(0).level == 20);
    CHECK(result->at(0).experience == 5000);
}

TEST_CASE("CharacterListUseCase — multiple characters", "[d2cs][char_list]") {
    InMemoryCharacterRepository repo;
    seed(repo, "bob", make_char("Barb1",  CharacterClass::Barbarian,   30, 10000));
    seed(repo, "bob", make_char("Necro1", CharacterClass::Necromancer, 15,  2000));
    seed(repo, "bob", make_char("Pala1",  CharacterClass::Paladin,     25,  7500));

    CharacterListUseCase uc{repo};
    auto result = uc.execute("bob");

    REQUIRE(result.has_value());
    CHECK(result->size() == 3);
}

TEST_CASE("CharacterListUseCase — different accounts are isolated", "[d2cs][char_list]") {
    InMemoryCharacterRepository repo;
    seed(repo, "alice", make_char("AliceChar"));
    seed(repo, "bob",   make_char("BobChar"));

    CharacterListUseCase uc{repo};

    auto alice_chars = uc.execute("alice");
    REQUIRE(alice_chars.has_value());
    REQUIRE(alice_chars->size() == 1);
    CHECK(alice_chars->at(0).name == "AliceChar");

    auto bob_chars = uc.execute("bob");
    REQUIRE(bob_chars.has_value());
    REQUIRE(bob_chars->size() == 1);
    CHECK(bob_chars->at(0).name == "BobChar");
}

// ===========================================================================
// CharacterSelectUseCase
// ===========================================================================

TEST_CASE("CharacterSelectUseCase — found", "[d2cs][char_select]") {
    InMemoryCharacterRepository repo;
    seed(repo, "alice", make_char("MyAmazon", CharacterClass::Amazon, 40, 20000));

    CharacterSelectUseCase uc{repo};
    auto result = uc.execute("alice", "MyAmazon");

    REQUIRE(result.has_value());
    CHECK(result->name == "MyAmazon");
    CHECK(result->class_ == CharacterClass::Amazon);
    CHECK(result->level == 40);
    CHECK(result->experience == 20000);
}

TEST_CASE("CharacterSelectUseCase — not found (wrong char name)", "[d2cs][char_select]") {
    InMemoryCharacterRepository repo;
    seed(repo, "alice", make_char("MyAmazon"));

    CharacterSelectUseCase uc{repo};
    auto result = uc.execute("alice", "NonExistent");

    CHECK_FALSE(result.has_value());
}

TEST_CASE("CharacterSelectUseCase — not found (wrong account)", "[d2cs][char_select]") {
    InMemoryCharacterRepository repo;
    seed(repo, "alice", make_char("MyAmazon"));

    CharacterSelectUseCase uc{repo};
    auto result = uc.execute("bob", "MyAmazon");

    CHECK_FALSE(result.has_value());
}

TEST_CASE("CharacterSelectUseCase — selects correct char among multiple", "[d2cs][char_select]") {
    InMemoryCharacterRepository repo;
    seed(repo, "alice", make_char("Char1", CharacterClass::Sorceress, 10, 100));
    seed(repo, "alice", make_char("Char2", CharacterClass::Barbarian, 20, 200));
    seed(repo, "alice", make_char("Char3", CharacterClass::Paladin,   30, 300));

    CharacterSelectUseCase uc{repo};
    auto result = uc.execute("alice", "Char2");

    REQUIRE(result.has_value());
    CHECK(result->name == "Char2");
    CHECK(result->class_ == CharacterClass::Barbarian);
    CHECK(result->level == 20);
}

// ===========================================================================
// CharacterCreateUseCase
// ===========================================================================

TEST_CASE("CharacterCreateUseCase — success", "[d2cs][char_create]") {
    InMemoryCharacterRepository repo;
    CharacterCreateUseCase uc{repo};

    bool ok = uc.execute("alice", make_char("NewChar", CharacterClass::Druid, 1, 0));
    REQUIRE(ok);

    // Verify it was actually stored
    auto found = repo.find_character("alice", "NewChar");
    REQUIRE(found.has_value());
    CHECK(found->class_ == CharacterClass::Druid);
}

TEST_CASE("CharacterCreateUseCase — duplicate name returns false", "[d2cs][char_create]") {
    InMemoryCharacterRepository repo;
    seed(repo, "alice", make_char("Existing"));

    CharacterCreateUseCase uc{repo};
    bool ok = uc.execute("alice", make_char("Existing"));

    CHECK_FALSE(ok);
    // Original should be unchanged
    auto found = repo.find_character("alice", "Existing");
    REQUIRE(found.has_value());
}

TEST_CASE("CharacterCreateUseCase — name too short (1 char) returns false", "[d2cs][char_create]") {
    InMemoryCharacterRepository repo;
    CharacterCreateUseCase uc{repo};

    bool ok = uc.execute("alice", make_char("X"));
    CHECK_FALSE(ok);
    CHECK(repo.total_character_count() == 0);
}

TEST_CASE("CharacterCreateUseCase — name too long (16 chars) returns false", "[d2cs][char_create]") {
    InMemoryCharacterRepository repo;
    CharacterCreateUseCase uc{repo};

    bool ok = uc.execute("alice", make_char("TooLongCharName1"));  // 16 chars
    CHECK_FALSE(ok);
    CHECK(repo.total_character_count() == 0);
}

TEST_CASE("CharacterCreateUseCase — name with invalid chars returns false", "[d2cs][char_create]") {
    InMemoryCharacterRepository repo;
    CharacterCreateUseCase uc{repo};

    // Hyphen is not allowed
    bool ok = uc.execute("alice", make_char("Bad-Name"));
    CHECK_FALSE(ok);
    CHECK(repo.total_character_count() == 0);
}

TEST_CASE("CharacterCreateUseCase — name with space returns false", "[d2cs][char_create]") {
    InMemoryCharacterRepository repo;
    CharacterCreateUseCase uc{repo};

    bool ok = uc.execute("alice", make_char("Bad Name"));
    CHECK_FALSE(ok);
    CHECK(repo.total_character_count() == 0);
}

TEST_CASE("CharacterCreateUseCase — valid boundary names (2 and 15 chars)", "[d2cs][char_create]") {
    InMemoryCharacterRepository repo;
    CharacterCreateUseCase uc{repo};

    // Exactly 2 chars — valid
    bool ok2 = uc.execute("alice", make_char("Ab"));
    CHECK(ok2);

    // Exactly 15 chars — valid
    bool ok15 = uc.execute("alice", make_char("AbcdefghijklmNo"));  // 15 chars
    CHECK(ok15);

    CHECK(repo.total_character_count() == 2);
}

TEST_CASE("CharacterCreateUseCase — underscore in name is valid", "[d2cs][char_create]") {
    InMemoryCharacterRepository repo;
    CharacterCreateUseCase uc{repo};

    bool ok = uc.execute("alice", make_char("My_Char"));
    CHECK(ok);
    CHECK(repo.total_character_count() == 1);
}

// ===========================================================================
// CharacterDeleteUseCase
// ===========================================================================

TEST_CASE("CharacterDeleteUseCase — success", "[d2cs][char_delete]") {
    InMemoryCharacterRepository repo;
    seed(repo, "alice", make_char("ToDelete"));
    seed(repo, "alice", make_char("ToKeep"));

    CharacterDeleteUseCase uc{repo};
    bool ok = uc.execute("alice", "ToDelete");

    REQUIRE(ok);
    CHECK_FALSE(repo.find_character("alice", "ToDelete").has_value());
    CHECK(repo.find_character("alice", "ToKeep").has_value());
    CHECK(repo.total_character_count() == 1);
}

TEST_CASE("CharacterDeleteUseCase — not found returns false", "[d2cs][char_delete]") {
    InMemoryCharacterRepository repo;
    CharacterDeleteUseCase uc{repo};

    bool ok = uc.execute("alice", "NonExistent");
    CHECK_FALSE(ok);
}

TEST_CASE("CharacterDeleteUseCase — wrong account returns false", "[d2cs][char_delete]") {
    InMemoryCharacterRepository repo;
    seed(repo, "alice", make_char("AliceChar"));

    CharacterDeleteUseCase uc{repo};
    bool ok = uc.execute("bob", "AliceChar");

    CHECK_FALSE(ok);
    // Alice's character should still exist
    CHECK(repo.find_character("alice", "AliceChar").has_value());
}

// ===========================================================================
// LadderQueryUseCase
// ===========================================================================

TEST_CASE("LadderQueryUseCase — empty ladder returns empty list", "[d2cs][ladder]") {
    InMemoryLadderRepository repo;
    LadderQueryUseCase uc{repo};

    auto result = uc.execute(LadderType::Standard, 0, 10);
    REQUIRE(result.has_value());
    CHECK(result->empty());
}

TEST_CASE("LadderQueryUseCase — populated ladder returns sorted entries", "[d2cs][ladder]") {
    InMemoryLadderRepository repo;
    repo.add_entry(LadderType::Standard, make_ladder_entry("Char_C", "acc3", 1000));
    repo.add_entry(LadderType::Standard, make_ladder_entry("Char_A", "acc1", 9000));
    repo.add_entry(LadderType::Standard, make_ladder_entry("Char_B", "acc2", 5000));

    LadderQueryUseCase uc{repo};
    auto result = uc.execute(LadderType::Standard, 0, 10);

    REQUIRE(result.has_value());
    REQUIRE(result->size() == 3);
    // Sorted by XP descending
    CHECK(result->at(0).character_name == "Char_A");
    CHECK(result->at(0).rank == 1);
    CHECK(result->at(0).experience == 9000);
    CHECK(result->at(1).character_name == "Char_B");
    CHECK(result->at(1).rank == 2);
    CHECK(result->at(2).character_name == "Char_C");
    CHECK(result->at(2).rank == 3);
}

TEST_CASE("LadderQueryUseCase — pagination with start_pos", "[d2cs][ladder]") {
    InMemoryLadderRepository repo;
    for (uint32_t i = 0; i < 10; ++i) {
        repo.add_entry(LadderType::Standard,
            make_ladder_entry("Char" + std::to_string(i), "acc", (10 - i) * 1000));
    }

    LadderQueryUseCase uc{repo};

    // First page: entries 0–4
    auto page1 = uc.execute(LadderType::Standard, 0, 5);
    REQUIRE(page1.has_value());
    CHECK(page1->size() == 5);
    CHECK(page1->at(0).rank == 1);
    CHECK(page1->at(4).rank == 5);

    // Second page: entries 5–9
    auto page2 = uc.execute(LadderType::Standard, 5, 5);
    REQUIRE(page2.has_value());
    CHECK(page2->size() == 5);
    CHECK(page2->at(0).rank == 6);
    CHECK(page2->at(4).rank == 10);
}

TEST_CASE("LadderQueryUseCase — start_pos beyond end returns empty", "[d2cs][ladder]") {
    InMemoryLadderRepository repo;
    repo.add_entry(LadderType::Standard, make_ladder_entry("OnlyChar", "acc", 1000));

    LadderQueryUseCase uc{repo};
    auto result = uc.execute(LadderType::Standard, 100, 10);

    REQUIRE(result.has_value());
    CHECK(result->empty());
}

TEST_CASE("LadderQueryUseCase — different ladder types are independent", "[d2cs][ladder]") {
    InMemoryLadderRepository repo;
    repo.add_entry(LadderType::Standard,  make_ladder_entry("SoftChar", "acc", 5000));
    repo.add_entry(LadderType::Hardcore,  make_ladder_entry("HardChar", "acc", 8000));
    repo.add_entry(LadderType::Expansion, make_ladder_entry("ExpChar",  "acc", 3000));

    LadderQueryUseCase uc{repo};

    auto std_ladder  = uc.execute(LadderType::Standard,          0, 10);
    auto hard_ladder = uc.execute(LadderType::Hardcore,          0, 10);
    auto exp_ladder  = uc.execute(LadderType::Expansion,         0, 10);
    auto exph_ladder = uc.execute(LadderType::ExpansionHardcore, 0, 10);

    REQUIRE(std_ladder.has_value());
    REQUIRE(std_ladder->size() == 1);
    CHECK(std_ladder->at(0).character_name == "SoftChar");

    REQUIRE(hard_ladder.has_value());
    REQUIRE(hard_ladder->size() == 1);
    CHECK(hard_ladder->at(0).character_name == "HardChar");

    REQUIRE(exp_ladder.has_value());
    REQUIRE(exp_ladder->size() == 1);
    CHECK(exp_ladder->at(0).character_name == "ExpChar");

    REQUIRE(exph_ladder.has_value());
    CHECK(exph_ladder->empty());
}

TEST_CASE("LadderQueryUseCase — count limits results", "[d2cs][ladder]") {
    InMemoryLadderRepository repo;
    for (uint32_t i = 0; i < 20; ++i) {
        repo.add_entry(LadderType::Standard,
            make_ladder_entry("C" + std::to_string(i), "acc", i * 100));
    }

    LadderQueryUseCase uc{repo};
    auto result = uc.execute(LadderType::Standard, 0, 5);

    REQUIRE(result.has_value());
    CHECK(result->size() == 5);
}

// ===========================================================================
// InMemoryLadderRepository — get_character_ladder_entry
// ===========================================================================

TEST_CASE("InMemoryLadderRepository — get_character_ladder_entry found", "[d2cs][ladder_repo]") {
    InMemoryLadderRepository repo;
    repo.add_entry(LadderType::Standard, make_ladder_entry("Hero",     "acc1", 9000));
    repo.add_entry(LadderType::Standard, make_ladder_entry("Sidekick", "acc2", 3000));

    auto entry = repo.get_character_ladder_entry("Sidekick", LadderType::Standard);
    REQUIRE(entry.has_value());
    CHECK(entry->character_name == "Sidekick");
    CHECK(entry->rank == 2);
    CHECK(entry->experience == 3000);
}

TEST_CASE("InMemoryLadderRepository — get_character_ladder_entry not found", "[d2cs][ladder_repo]") {
    InMemoryLadderRepository repo;
    repo.add_entry(LadderType::Standard, make_ladder_entry("Hero", "acc1", 9000));

    auto entry = repo.get_character_ladder_entry("Ghost", LadderType::Standard);
    CHECK_FALSE(entry.has_value());
}

TEST_CASE("InMemoryLadderRepository — remove_entry reranks remaining", "[d2cs][ladder_repo]") {
    InMemoryLadderRepository repo;
    repo.add_entry(LadderType::Standard, make_ladder_entry("Rank1", "acc", 9000));
    repo.add_entry(LadderType::Standard, make_ladder_entry("Rank2", "acc", 6000));
    repo.add_entry(LadderType::Standard, make_ladder_entry("Rank3", "acc", 3000));

    bool removed = repo.remove_entry(LadderType::Standard, "Rank1");
    REQUIRE(removed);
    CHECK(repo.entry_count(LadderType::Standard) == 2);

    // Rank2 should now be rank 1
    auto entry = repo.get_character_ladder_entry("Rank2", LadderType::Standard);
    REQUIRE(entry.has_value());
    CHECK(entry->rank == 1);

    auto entry3 = repo.get_character_ladder_entry("Rank3", LadderType::Standard);
    REQUIRE(entry3.has_value());
    CHECK(entry3->rank == 2);
}

// ===========================================================================
// CharacterFlags bitmask operations
// ===========================================================================

TEST_CASE("CharacterFlags — bitmask operations", "[d2cs][types]") {
    auto flags = CharacterFlags::Hardcore | CharacterFlags::Expansion;

    CHECK(has_flag(flags, CharacterFlags::Hardcore));
    CHECK(has_flag(flags, CharacterFlags::Expansion));
    CHECK_FALSE(has_flag(flags, CharacterFlags::Died));
    CHECK_FALSE(has_flag(flags, CharacterFlags::Ladder));
}

TEST_CASE("CharacterFlags — None has no bits set", "[d2cs][types]") {
    auto flags = CharacterFlags::None;
    CHECK_FALSE(has_flag(flags, CharacterFlags::Hardcore));
    CHECK_FALSE(has_flag(flags, CharacterFlags::Died));
    CHECK_FALSE(has_flag(flags, CharacterFlags::Expansion));
    CHECK_FALSE(has_flag(flags, CharacterFlags::Ladder));
}

TEST_CASE("CharacterFlags — all flags combined", "[d2cs][types]") {
    auto flags = CharacterFlags::Hardcore
               | CharacterFlags::Died
               | CharacterFlags::Expansion
               | CharacterFlags::Ladder;

    CHECK(has_flag(flags, CharacterFlags::Hardcore));
    CHECK(has_flag(flags, CharacterFlags::Died));
    CHECK(has_flag(flags, CharacterFlags::Expansion));
    CHECK(has_flag(flags, CharacterFlags::Ladder));
}

// ===========================================================================
// Name validation edge cases (via CharacterCreateUseCase)
// ===========================================================================

TEST_CASE("CharacterCreateUseCase — numeric-only name is valid", "[d2cs][char_create][validation]") {
    InMemoryCharacterRepository repo;
    CharacterCreateUseCase uc{repo};

    bool ok = uc.execute("alice", make_char("12345"));
    CHECK(ok);
}

TEST_CASE("CharacterCreateUseCase — empty name returns false", "[d2cs][char_create][validation]") {
    InMemoryCharacterRepository repo;
    CharacterCreateUseCase uc{repo};

    bool ok = uc.execute("alice", make_char(""));
    CHECK_FALSE(ok);
}

TEST_CASE("CharacterCreateUseCase — name with dot returns false", "[d2cs][char_create][validation]") {
    InMemoryCharacterRepository repo;
    CharacterCreateUseCase uc{repo};

    bool ok = uc.execute("alice", make_char("Bad.Name"));
    CHECK_FALSE(ok);
}
