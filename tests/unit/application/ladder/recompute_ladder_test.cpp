// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::ladder::RecomputeLadder`.
// Uses inline fakes for ILadderRepository.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include "application/ladder/recompute_ladder.hpp"
#include "domain/ladder/ports.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/ladder/ladder.hpp"
#include "domain/shared/ids.hpp"

namespace {

using namespace pvpgn;
using application::ladder::RecomputeLadder;
using application::ladder::RecomputeLadderCommand;

// ---------------------------------------------------------------------------
// Inline fake: ILadderRepository
// ---------------------------------------------------------------------------
class FakeLadderRepository final : public domain::ladder::ILadderRepository {
public:
    std::vector<domain::ladder::LadderEntry> entries;

    core::Result<uint32_t, core::Error>
    get_rank(domain::AccountId account_id) override {
        for (uint32_t i = 0; i < entries.size(); ++i) {
            // rank by position (entries assumed sorted)
            (void)account_id;
        }
        return core::fail(core::Error{core::StatusCode::NotFound, "not found"});
    }

    core::Result<void, core::Error>
    save_entry(const domain::ladder::LadderEntry& entry) override {
        // Remove any existing entry for this account, then append — so the
        // stored order reflects the order entries were saved in. RecomputeLadder
        // re-saves entries in rank order, so the final vector ends up ranked,
        // which is what the happy-path test asserts.
        std::erase_if(entries, [&](const domain::ladder::LadderEntry& e) {
            return e.account == entry.account;
        });
        entries.push_back(entry);
        return core::ok();
    }

    core::Result<std::vector<domain::ladder::LadderEntry>, core::Error>
    get_top_n(uint32_t n) override {
        auto copy = entries;
        if (copy.size() > n) copy.resize(n);
        return copy;
    }
};

}  // namespace

// ---------------------------------------------------------------------------
// Test cases
// ---------------------------------------------------------------------------

TEST_CASE("RecomputeLadder: happy path — entries ranked by rating descending",
          "[application][ladder][recompute]") {
    FakeLadderRepository repo;
    // Add entries out of order
    repo.entries = {
        {domain::AccountId{1}, 1500, 5, 2, 0},  // rating 1500
        {domain::AccountId{2}, 1600, 10, 1, 0}, // rating 1600 — should be rank 1
        {domain::AccountId{3}, 1400, 3, 4, 1},  // rating 1400
    };

    RecomputeLadder uc{repo};
    auto result = uc.execute(RecomputeLadderCommand{"STAR"});

    REQUIRE(result);
    REQUIRE(result.value().entries_updated == 3);

    // After recompute, entries should be sorted by rating descending
    REQUIRE(repo.entries[0].account == domain::AccountId{2});  // 1600
    REQUIRE(repo.entries[1].account == domain::AccountId{1});  // 1500
    REQUIRE(repo.entries[2].account == domain::AccountId{3});  // 1400
}

TEST_CASE("RecomputeLadder: rating is the primary key, wins only the tie-break",
          "[application][ladder][recompute]") {
    // A has a HIGHER rating but FEWER wins than B. The original rank-bearing
    // ladder (ladder_sort_highestrated) ranks by rating first, so A must
    // outrank B despite having fewer wins.
    FakeLadderRepository repo;
    repo.entries = {
        {domain::AccountId{10}, 1500, 2, 0, 0},   // B: lower rating, more wins
        {domain::AccountId{20}, 1800, 1, 0, 0},   // A: higher rating, fewer wins
    };

    RecomputeLadder uc{repo};
    auto result = uc.execute(RecomputeLadderCommand{"STAR"});

    REQUIRE(result);
    REQUIRE(result.value().entries_updated == 2);

    // A (higher rating) ranks above B (more wins).
    REQUIRE(repo.entries[0].account == domain::AccountId{20});  // rating 1800
    REQUIRE(repo.entries[1].account == domain::AccountId{10});  // rating 1500
}

TEST_CASE("RecomputeLadder: empty ladder_id returns InvalidArgument",
          "[application][ladder][recompute]") {
    FakeLadderRepository repo;
    repo.entries = {{domain::AccountId{1}, 1500, 5, 2, 0}};

    RecomputeLadder uc{repo};
    auto result = uc.execute(RecomputeLadderCommand{""});

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("RecomputeLadder: no entries returns 0 updated",
          "[application][ladder][recompute]") {
    FakeLadderRepository repo;
    // repo.entries is empty

    RecomputeLadder uc{repo};
    auto result = uc.execute(RecomputeLadderCommand{"WAR3"});

    REQUIRE(result);
    REQUIRE(result.value().entries_updated == 0);
}
