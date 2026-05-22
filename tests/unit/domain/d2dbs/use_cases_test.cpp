// SPDX-License-Identifier: GPL-2.0-or-later
/// @file use_cases_test.cpp
/// Unit tests for domain::d2dbs use cases and in-memory repositories.
///
/// Test count: 12 TEST_CASEs, 50+ CHECK/REQUIRE assertions.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "domain/d2dbs/in_memory_repositories.hpp"
#include "domain/d2dbs/types.hpp"
#include "domain/d2dbs/use_cases.hpp"

using namespace pvpgn::domain::d2dbs;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

/// Build a minimal CharacterSaveData.
CharacterSaveData make_save(std::string account,
                             std::string char_name,
                             std::string realm    = "USEast",
                             uint32_t    timestamp = 1000) {
    CharacterSaveData d;
    d.account_name = std::move(account);
    d.char_name    = std::move(char_name);
    d.realm_name   = std::move(realm);
    d.data         = {0x55, 0xAA, 0x01, 0x02};  // minimal blob
    d.timestamp    = timestamp;
    return d;
}

/// Build a minimal LadderUpdateEntry.
LadderUpdateEntry make_ladder(std::string char_name,
                               std::string account   = "alice",
                               uint64_t    xp        = 100000,
                               uint8_t     level     = 50,
                               uint8_t     cls       = 2) {
    LadderUpdateEntry e;
    e.char_name    = std::move(char_name);
    e.account_name = std::move(account);
    e.experience   = xp;
    e.level        = level;
    e.char_class   = cls;
    e.flags        = 0;
    return e;
}

} // namespace

// ===========================================================================
// CharacterSaveUseCase
// ===========================================================================

TEST_CASE("CharacterSaveUseCase — save returns true", "[d2dbs][char_save]") {
    InMemoryCharacterSaveRepository repo;
    CharacterSaveUseCase uc{repo};

    const bool ok = uc.execute(make_save("alice", "Sorc1"));
    REQUIRE(ok);
    CHECK(repo.save_count() == 1);
}

TEST_CASE("CharacterSaveUseCase — overwrite existing save", "[d2dbs][char_save]") {
    InMemoryCharacterSaveRepository repo;
    CharacterSaveUseCase uc{repo};

    auto first = make_save("alice", "Sorc1");
    first.data = {0x01};
    REQUIRE(uc.execute(first));

    auto second = make_save("alice", "Sorc1");
    second.data = {0x02, 0x03};
    REQUIRE(uc.execute(second));

    // Still only one entry (overwrite, not duplicate)
    CHECK(repo.save_count() == 1);
}

TEST_CASE("CharacterSaveUseCase — different characters are independent", "[d2dbs][char_save]") {
    InMemoryCharacterSaveRepository repo;
    CharacterSaveUseCase uc{repo};

    REQUIRE(uc.execute(make_save("alice", "Sorc1")));
    REQUIRE(uc.execute(make_save("alice", "Barb1")));
    REQUIRE(uc.execute(make_save("bob",   "Necro1")));

    CHECK(repo.save_count() == 3);
}

// ===========================================================================
// CharacterLoadUseCase
// ===========================================================================

TEST_CASE("CharacterLoadUseCase — load existing character", "[d2dbs][char_load]") {
    InMemoryCharacterSaveRepository repo;
    CharacterSaveUseCase  save_uc{repo};
    CharacterLoadUseCase  load_uc{repo};

    auto saved = make_save("alice", "Sorc1", "USEast", 9999);
    REQUIRE(save_uc.execute(saved));

    auto result = load_uc.execute("alice", "Sorc1");
    REQUIRE(result.has_value());
    CHECK(result->account_name == "alice");
    CHECK(result->char_name    == "Sorc1");
    CHECK(result->realm_name   == "USEast");
    CHECK(result->timestamp    == 9999);
    CHECK(result->data         == saved.data);
}

TEST_CASE("CharacterLoadUseCase — load non-existent returns nullopt", "[d2dbs][char_load]") {
    InMemoryCharacterSaveRepository repo;
    CharacterLoadUseCase uc{repo};

    auto result = uc.execute("alice", "NonExistent");
    CHECK_FALSE(result.has_value());
}

TEST_CASE("CharacterLoadUseCase — wrong account returns nullopt", "[d2dbs][char_load]") {
    InMemoryCharacterSaveRepository repo;
    CharacterSaveUseCase save_uc{repo};
    CharacterLoadUseCase load_uc{repo};

    REQUIRE(save_uc.execute(make_save("alice", "Sorc1")));

    // Same char name but different account
    auto result = load_uc.execute("bob", "Sorc1");
    CHECK_FALSE(result.has_value());
}

// ===========================================================================
// CharacterLockUseCase / CharacterUnlockUseCase
// ===========================================================================

TEST_CASE("CharacterLockUseCase — lock existing character succeeds", "[d2dbs][char_lock]") {
    InMemoryCharacterSaveRepository repo;
    CharacterSaveUseCase   save_uc{repo};
    CharacterLockUseCase   lock_uc{repo};

    REQUIRE(save_uc.execute(make_save("alice", "Sorc1")));

    const bool ok = lock_uc.execute("alice", "Sorc1");
    REQUIRE(ok);
    CHECK(repo.lock_count() == 1);
    CHECK(repo.lock_state("alice", "Sorc1") == CharacterLockState::Locked);
}

TEST_CASE("CharacterLockUseCase — lock non-existent character fails", "[d2dbs][char_lock]") {
    InMemoryCharacterSaveRepository repo;
    CharacterLockUseCase uc{repo};

    const bool ok = uc.execute("alice", "Ghost");
    CHECK_FALSE(ok);
    CHECK(repo.lock_count() == 0);
}

TEST_CASE("CharacterLockUseCase — double-lock fails", "[d2dbs][char_lock]") {
    InMemoryCharacterSaveRepository repo;
    CharacterSaveUseCase save_uc{repo};
    CharacterLockUseCase lock_uc{repo};

    REQUIRE(save_uc.execute(make_save("alice", "Sorc1")));
    REQUIRE(lock_uc.execute("alice", "Sorc1"));

    // Second lock attempt must fail
    const bool ok = lock_uc.execute("alice", "Sorc1");
    CHECK_FALSE(ok);
    CHECK(repo.lock_count() == 1);
}

TEST_CASE("CharacterUnlockUseCase — unlock locked character succeeds", "[d2dbs][char_lock]") {
    InMemoryCharacterSaveRepository repo;
    CharacterSaveUseCase   save_uc{repo};
    CharacterLockUseCase   lock_uc{repo};
    CharacterUnlockUseCase unlock_uc{repo};

    REQUIRE(save_uc.execute(make_save("alice", "Sorc1")));
    REQUIRE(lock_uc.execute("alice", "Sorc1"));

    const bool ok = unlock_uc.execute("alice", "Sorc1");
    REQUIRE(ok);
    CHECK(repo.lock_count() == 0);
    CHECK(repo.lock_state("alice", "Sorc1") == CharacterLockState::Unlocked);
}

TEST_CASE("CharacterUnlockUseCase — unlock non-locked character fails", "[d2dbs][char_lock]") {
    InMemoryCharacterSaveRepository repo;
    CharacterSaveUseCase   save_uc{repo};
    CharacterUnlockUseCase unlock_uc{repo};

    REQUIRE(save_uc.execute(make_save("alice", "Sorc1")));

    // Not locked — unlock should fail
    const bool ok = unlock_uc.execute("alice", "Sorc1");
    CHECK_FALSE(ok);
}

TEST_CASE("CharacterLockUseCase — lock/unlock/relock cycle", "[d2dbs][char_lock]") {
    InMemoryCharacterSaveRepository repo;
    CharacterSaveUseCase   save_uc{repo};
    CharacterLockUseCase   lock_uc{repo};
    CharacterUnlockUseCase unlock_uc{repo};

    REQUIRE(save_uc.execute(make_save("alice", "Sorc1")));

    REQUIRE(lock_uc.execute("alice", "Sorc1"));
    CHECK(repo.lock_state("alice", "Sorc1") == CharacterLockState::Locked);

    REQUIRE(unlock_uc.execute("alice", "Sorc1"));
    CHECK(repo.lock_state("alice", "Sorc1") == CharacterLockState::Unlocked);

    // Can lock again after unlock
    REQUIRE(lock_uc.execute("alice", "Sorc1"));
    CHECK(repo.lock_state("alice", "Sorc1") == CharacterLockState::Locked);
}

// ===========================================================================
// LadderUpdateUseCase
// ===========================================================================

TEST_CASE("LadderUpdateUseCase — insert new entry", "[d2dbs][ladder]") {
    InMemoryD2DBSLadderRepository repo;
    LadderUpdateUseCase uc{repo};

    const bool ok = uc.execute(make_ladder("Sorc1", "alice", 500000, 75, 3));
    REQUIRE(ok);
    CHECK(repo.entry_count() == 1);

    auto found = repo.find_entry("Sorc1");
    REQUIRE(found.has_value());
    CHECK(found->char_name    == "Sorc1");
    CHECK(found->account_name == "alice");
    CHECK(found->experience   == 500000);
    CHECK(found->level        == 75);
    CHECK(found->char_class   == 3);
}

TEST_CASE("LadderUpdateUseCase — update existing entry", "[d2dbs][ladder]") {
    InMemoryD2DBSLadderRepository repo;
    LadderUpdateUseCase uc{repo};

    REQUIRE(uc.execute(make_ladder("Sorc1", "alice", 100000, 50)));
    REQUIRE(uc.execute(make_ladder("Sorc1", "alice", 999999, 99)));

    // Still only one entry
    CHECK(repo.entry_count() == 1);

    auto found = repo.find_entry("Sorc1");
    REQUIRE(found.has_value());
    CHECK(found->experience == 999999);
    CHECK(found->level      == 99);
}

TEST_CASE("LadderUpdateUseCase — multiple characters", "[d2dbs][ladder]") {
    InMemoryD2DBSLadderRepository repo;
    LadderUpdateUseCase uc{repo};

    REQUIRE(uc.execute(make_ladder("Sorc1",  "alice", 500000)));
    REQUIRE(uc.execute(make_ladder("Barb1",  "bob",   300000)));
    REQUIRE(uc.execute(make_ladder("Necro1", "carol", 700000)));

    CHECK(repo.entry_count() == 3);

    CHECK(repo.find_entry("Sorc1").has_value());
    CHECK(repo.find_entry("Barb1").has_value());
    CHECK(repo.find_entry("Necro1").has_value());
    CHECK_FALSE(repo.find_entry("Ghost").has_value());
}
