// SPDX-License-Identifier: GPL-2.0-or-later
//
// Unit tests for the `D2Ladder` aggregate (src/domain/ladder/d2_ladder.cpp).
// The aggregate's invariants are:
//   - entries are always sorted by experience descending,
//   - no two entries share a char_name,
//   - size never exceeds max_entries() (1000 overall / 200 per-class).
// These tests pin construction, the sorted-insert path, duplicate/full
// rejection, removal, and the rank_of/top queries.

#include <catch2/catch_test_macros.hpp>

#include "domain/ladder/d2_ladder.hpp"

using namespace pvpgn;
using domain::ladder::D2Ladder;
using domain::ladder::D2LadderEntry;
using domain::ladder::D2LadderType;
using domain::ladder::kMaxClassLadderEntries;
using domain::ladder::kMaxOverallLadderEntries;
using core::StatusCode;

namespace {

D2LadderEntry make_entry(std::string name, std::uint32_t experience,
                         std::uint8_t level = 1) {
    D2LadderEntry e;
    e.char_name    = std::move(name);
    e.account_name = "acct";
    e.experience   = experience;
    e.level        = level;
    return e;
}

}  // namespace

TEST_CASE("D2Ladder: overall type gets the 1000-entry cap", "[domain][d2ladder]") {
    D2Ladder l{D2LadderType::std_overall};
    CHECK(l.type() == D2LadderType::std_overall);
    CHECK(l.max_entries() == kMaxOverallLadderEntries);
    CHECK(l.entries().empty());
    CHECK_FALSE(l.is_full());
}

TEST_CASE("D2Ladder: class type gets the 200-entry cap", "[domain][d2ladder]") {
    D2Ladder l{D2LadderType::exp_hc_assassin};
    CHECK(l.max_entries() == kMaxClassLadderEntries);
}

TEST_CASE("D2Ladder: add keeps entries sorted by experience descending",
          "[domain][d2ladder]") {
    D2Ladder l{D2LadderType::std_amazon};
    REQUIRE(l.add(make_entry("mid", 500)).has_value());
    REQUIRE(l.add(make_entry("low", 100)).has_value());
    REQUIRE(l.add(make_entry("high", 900)).has_value());

    auto const ents = l.entries();
    REQUIRE(ents.size() == 3);
    CHECK(ents[0].char_name == "high");
    CHECK(ents[1].char_name == "mid");
    CHECK(ents[2].char_name == "low");
}

TEST_CASE("D2Ladder: rank_of is 1-based and reflects sort order",
          "[domain][d2ladder]") {
    D2Ladder l{D2LadderType::std_amazon};
    REQUIRE(l.add(make_entry("a", 300)).has_value());
    REQUIRE(l.add(make_entry("b", 800)).has_value());
    REQUIRE(l.add(make_entry("c", 500)).has_value());

    CHECK(l.rank_of("b") == 1u);  // highest experience
    CHECK(l.rank_of("c") == 2u);
    CHECK(l.rank_of("a") == 3u);
    CHECK_FALSE(l.rank_of("missing").has_value());
}

TEST_CASE("D2Ladder: duplicate char_name is rejected with AlreadyExists",
          "[domain][d2ladder]") {
    D2Ladder l{D2LadderType::std_amazon};
    REQUIRE(l.add(make_entry("dup", 100)).has_value());

    auto const r = l.add(make_entry("dup", 999));
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().code() == StatusCode::AlreadyExists);
    // The rejected add must not have mutated the ladder.
    CHECK(l.entries().size() == 1);
    CHECK(l.entries()[0].experience == 100);
}

TEST_CASE("D2Ladder: add past capacity is rejected with ResourceExhausted",
          "[domain][d2ladder]") {
    D2Ladder l{D2LadderType::std_amazon};
    for (std::size_t i = 0; i < kMaxClassLadderEntries; ++i) {
        REQUIRE(l.add(make_entry("c" + std::to_string(i),
                                 static_cast<std::uint32_t>(i))).has_value());
    }
    REQUIRE(l.is_full());

    auto const r = l.add(make_entry("overflow", 1));
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().code() == StatusCode::ResourceExhausted);
    CHECK(l.entries().size() == kMaxClassLadderEntries);
}

TEST_CASE("D2Ladder: remove deletes an entry and re-ranks the rest",
          "[domain][d2ladder]") {
    D2Ladder l{D2LadderType::std_amazon};
    REQUIRE(l.add(make_entry("top", 900)).has_value());
    REQUIRE(l.add(make_entry("mid", 500)).has_value());
    REQUIRE(l.add(make_entry("bot", 100)).has_value());

    REQUIRE(l.remove("mid").has_value());
    REQUIRE(l.entries().size() == 2);
    CHECK(l.rank_of("top") == 1u);
    CHECK(l.rank_of("bot") == 2u);
    CHECK_FALSE(l.rank_of("mid").has_value());
}

TEST_CASE("D2Ladder: remove of an absent name is NotFound", "[domain][d2ladder]") {
    D2Ladder l{D2LadderType::std_amazon};
    REQUIRE(l.add(make_entry("only", 1)).has_value());

    auto const r = l.remove("ghost");
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().code() == StatusCode::NotFound);
    CHECK(l.entries().size() == 1);
}

TEST_CASE("D2Ladder: top(n) clamps to available entries", "[domain][d2ladder]") {
    D2Ladder l{D2LadderType::std_amazon};
    REQUIRE(l.add(make_entry("a", 300)).has_value());
    REQUIRE(l.add(make_entry("b", 200)).has_value());
    REQUIRE(l.add(make_entry("c", 100)).has_value());

    CHECK(l.top(0).empty());
    CHECK(l.top(2).size() == 2);
    CHECK(l.top(2)[0].char_name == "a");
    CHECK(l.top(2)[1].char_name == "b");
    // Over-requesting returns everything, not garbage.
    CHECK(l.top(99).size() == 3);
}
