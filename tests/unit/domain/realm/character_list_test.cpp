// SPDX-License-Identifier: GPL-2.0-or-later
//
// Unit tests for the `CharacterList` aggregate (src/domain/realm/character_list.cpp).
// Invariants pinned here:
//   - count() never exceeds max_capacity(),
//   - no two characters share a char_name,
//   - add/remove/find report ResourceExhausted/AlreadyExists/NotFound,
//   - list(SortMode) orders by each of the four sort keys.

#include <chrono>

#include <catch2/catch_test_macros.hpp>

#include "core/clock.hpp"
#include "domain/realm/character.hpp"
#include "domain/realm/character_list.hpp"

namespace pvpgn::domain::realm {
namespace {

// Fixed epoch for deterministic creation/last-played timestamps.
const core::SystemTime kEpoch{};

Character make_char(std::string char_name, std::uint32_t level = 1,
                    core::SystemTime created = kEpoch,
                    core::SystemTime played  = kEpoch) {
    CharacterId   id{"acct", std::move(char_name)};
    CharacterStats stats;
    stats.level = level;
    Character c(id, stats, created);
    c.touch(played);
    return c;
}

}  // namespace

TEST_CASE("CharacterList: empty defaults", "[domain][realm][charlist]") {
    CharacterList list{"acct"};
    CHECK(list.account_name() == "acct");
    CHECK(list.count() == 0);
    CHECK(list.max_capacity() == 8);  // documented default
    CHECK_FALSE(list.is_full());
}

TEST_CASE("CharacterList: add then find returns the character",
          "[domain][realm][charlist]") {
    CharacterList list{"acct"};
    REQUIRE(list.add(make_char("Hero")).has_value());
    CHECK(list.count() == 1);

    auto const found = list.find("Hero");
    REQUIRE(found.has_value());
    CHECK(found.value()->id().char_name == "Hero");
}

TEST_CASE("CharacterList: find of an absent name is NotFound",
          "[domain][realm][charlist]") {
    CharacterList list{"acct"};
    REQUIRE(list.add(make_char("Hero")).has_value());

    auto const missing = list.find("Ghost");
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error().code() == core::StatusCode::NotFound);
}

TEST_CASE("CharacterList: duplicate char_name is rejected with AlreadyExists",
          "[domain][realm][charlist]") {
    CharacterList list{"acct"};
    REQUIRE(list.add(make_char("Dup", 5)).has_value());

    auto const r = list.add(make_char("Dup", 99));
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().code() == core::StatusCode::AlreadyExists);
    // Rejected add must not mutate the list.
    CHECK(list.count() == 1);
    CHECK(list.find("Dup").value()->stats().level == 5u);
}

TEST_CASE("CharacterList: add past capacity is ResourceExhausted",
          "[domain][realm][charlist]") {
    CharacterList list{"acct", 2};  // small cap
    REQUIRE(list.add(make_char("A")).has_value());
    REQUIRE(list.add(make_char("B")).has_value());
    REQUIRE(list.is_full());

    auto const r = list.add(make_char("C"));
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().code() == core::StatusCode::ResourceExhausted);
    CHECK(list.count() == 2);
}

TEST_CASE("CharacterList: remove deletes the named character",
          "[domain][realm][charlist]") {
    CharacterList list{"acct"};
    REQUIRE(list.add(make_char("Keep")).has_value());
    REQUIRE(list.add(make_char("Drop")).has_value());

    REQUIRE(list.remove("Drop").has_value());
    CHECK(list.count() == 1);
    CHECK_FALSE(list.find("Drop").has_value());
    CHECK(list.find("Keep").has_value());
}

TEST_CASE("CharacterList: remove of an absent name is NotFound",
          "[domain][realm][charlist]") {
    CharacterList list{"acct"};
    REQUIRE(list.add(make_char("Only")).has_value());

    auto const r = list.remove("Ghost");
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().code() == core::StatusCode::NotFound);
    CHECK(list.count() == 1);
}

TEST_CASE("CharacterList: list(by_name) sorts ascending lexicographically",
          "[domain][realm][charlist]") {
    CharacterList list{"acct"};
    REQUIRE(list.add(make_char("Charlie")).has_value());
    REQUIRE(list.add(make_char("Alice")).has_value());
    REQUIRE(list.add(make_char("Bob")).has_value());

    auto const view = list.list(SortMode::by_name);
    REQUIRE(view.size() == 3);
    CHECK(view[0]->id().char_name == "Alice");
    CHECK(view[1]->id().char_name == "Bob");
    CHECK(view[2]->id().char_name == "Charlie");
}

TEST_CASE("CharacterList: list(by_level) sorts descending by level",
          "[domain][realm][charlist]") {
    CharacterList list{"acct"};
    REQUIRE(list.add(make_char("Low", 5)).has_value());
    REQUIRE(list.add(make_char("High", 90)).has_value());
    REQUIRE(list.add(make_char("Mid", 40)).has_value());

    auto const view = list.list(SortMode::by_level);
    REQUIRE(view.size() == 3);
    CHECK(view[0]->id().char_name == "High");
    CHECK(view[1]->id().char_name == "Mid");
    CHECK(view[2]->id().char_name == "Low");
}

TEST_CASE("CharacterList: list(by_creation_time) puts oldest first",
          "[domain][realm][charlist]") {
    using namespace std::chrono;
    CharacterList list{"acct"};
    REQUIRE(list.add(make_char("Newest", 1, kEpoch + seconds{30})).has_value());
    REQUIRE(list.add(make_char("Oldest", 1, kEpoch + seconds{1})).has_value());
    REQUIRE(list.add(make_char("Middle", 1, kEpoch + seconds{10})).has_value());

    auto const view = list.list(SortMode::by_creation_time);
    REQUIRE(view.size() == 3);
    CHECK(view[0]->id().char_name == "Oldest");
    CHECK(view[1]->id().char_name == "Middle");
    CHECK(view[2]->id().char_name == "Newest");
}

TEST_CASE("CharacterList: list(by_last_played) puts most-recent first",
          "[domain][realm][charlist]") {
    using namespace std::chrono;
    CharacterList list{"acct"};
    REQUIRE(list.add(make_char("Stale", 1, kEpoch, kEpoch + seconds{1})).has_value());
    REQUIRE(list.add(make_char("Fresh", 1, kEpoch, kEpoch + seconds{50})).has_value());
    REQUIRE(list.add(make_char("Recent", 1, kEpoch, kEpoch + seconds{20})).has_value());

    auto const view = list.list(SortMode::by_last_played);
    REQUIRE(view.size() == 3);
    CHECK(view[0]->id().char_name == "Fresh");
    CHECK(view[1]->id().char_name == "Recent");
    CHECK(view[2]->id().char_name == "Stale");
}

}  // namespace pvpgn::domain::realm
