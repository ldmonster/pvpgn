// SPDX-License-Identifier: GPL-2.0-or-later

/// @file user_name_exhaustive_test.cpp
/// Net-new branch coverage for domain::UserName beyond user_name_test.cpp.
/// Targets: error StatusCode/message on each rejection branch, every legal
/// character class as the *first* character, length boundaries one step
/// either side, canonical/display interaction with hash and unordered_set,
/// self-equality and copy semantics.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <unordered_set>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/user_name.hpp"

using pvpgn::core::StatusCode;
using pvpgn::domain::UserName;

// ---------------------------------------------------------------------------
// Length-boundary branches (kMin/kMax constants)
// ---------------------------------------------------------------------------

TEST_CASE("UserName: kMin and kMax constants are 2 and 15",
          "[domain][shared][user_name]") {
    REQUIRE(UserName::kMin == 2u);
    REQUIRE(UserName::kMax == 15u);
}

TEST_CASE("UserName: parse rejects length just below minimum carries InvalidArgument",
          "[domain][shared][user_name]") {
    auto r = UserName::parse("A");  // 1 char < kMin
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == StatusCode::InvalidArgument);
}

TEST_CASE("UserName: parse rejects length just above maximum carries InvalidArgument",
          "[domain][shared][user_name]") {
    auto r = UserName::parse("Abcdefghijklmnop");  // 16 chars > kMax
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == StatusCode::InvalidArgument);
}

TEST_CASE("UserName: parse accepts exactly minimum and maximum length",
          "[domain][shared][user_name]") {
    REQUIRE(UserName::parse("Ab").has_value());                 // exactly kMin
    REQUIRE(UserName::parse("Abcdefghijklmno").has_value());    // exactly kMax (15)
}

// ---------------------------------------------------------------------------
// First-character branch: must be alpha
// ---------------------------------------------------------------------------

TEST_CASE("UserName: parse accepts lowercase first letter",
          "[domain][shared][user_name]") {
    REQUIRE(UserName::parse("zebra").has_value());
}

TEST_CASE("UserName: parse accepts uppercase first letter",
          "[domain][shared][user_name]") {
    REQUIRE(UserName::parse("Zebra").has_value());
}

TEST_CASE("UserName: parse rejects dash as first character with InvalidArgument",
          "[domain][shared][user_name]") {
    auto r = UserName::parse("-bob");
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == StatusCode::InvalidArgument);
}

TEST_CASE("UserName: parse rejects dot as first character",
          "[domain][shared][user_name]") {
    REQUIRE_FALSE(UserName::parse(".bob").has_value());
}

TEST_CASE("UserName: parse rejects digit first character carries InvalidArgument",
          "[domain][shared][user_name]") {
    auto r = UserName::parse("9bob");
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == StatusCode::InvalidArgument);
}

// ---------------------------------------------------------------------------
// Illegal-character branch (non-first position)
// ---------------------------------------------------------------------------

TEST_CASE("UserName: parse rejects leading-letter then illegal char carries InvalidArgument",
          "[domain][shared][user_name]") {
    auto r = UserName::parse("Bob!");
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == StatusCode::InvalidArgument);
}

TEST_CASE("UserName: parse rejects trailing space",
          "[domain][shared][user_name]") {
    REQUIRE_FALSE(UserName::parse("Bob ").has_value());
}

TEST_CASE("UserName: parse rejects leading space (also fails first-letter rule)",
          "[domain][shared][user_name]") {
    REQUIRE_FALSE(UserName::parse(" Bob").has_value());
}

TEST_CASE("UserName: parse rejects high-bit / non-ASCII byte",
          "[domain][shared][user_name]") {
    REQUIRE_FALSE(UserName::parse(std::string_view{"Bo\xC3", 3}).has_value());
}

TEST_CASE("UserName: parse accepts all legal punctuation in interior",
          "[domain][shared][user_name]") {
    REQUIRE(UserName::parse("a_-.b").has_value());
}

// ---------------------------------------------------------------------------
// canonical / display behaviour
// ---------------------------------------------------------------------------

TEST_CASE("UserName: digits and punctuation are unchanged by canonicalisation",
          "[domain][shared][user_name]") {
    auto u = UserName::parse("Bob_99-X.Y").value();
    REQUIRE(std::string{u.canonical()} == "bob_99-x.y");
    REQUIRE(std::string{u.display()}   == "Bob_99-X.Y");
}

TEST_CASE("UserName: already-lowercase name has identical display and canonical",
          "[domain][shared][user_name]") {
    auto u = UserName::parse("plainname").value();
    REQUIRE(u.display() == u.canonical());
}

// ---------------------------------------------------------------------------
// Equality / inequality additional branches
// ---------------------------------------------------------------------------

TEST_CASE("UserName: name is equal to itself (reflexive)",
          "[domain][shared][user_name]") {
    auto u = UserName::parse("Alice").value();
    REQUIRE(u == u);
    REQUIRE_FALSE(u != u);
}

TEST_CASE("UserName: copy compares equal to original",
          "[domain][shared][user_name]") {
    auto u = UserName::parse("Alice").value();
    UserName copy = u;
    REQUIRE(copy == u);
}

TEST_CASE("UserName: names differing only in punctuation are unequal",
          "[domain][shared][user_name]") {
    auto a = UserName::parse("a_b").value();
    auto b = UserName::parse("a-b").value();
    REQUIRE(a != b);
}

// ---------------------------------------------------------------------------
// std::hash usability in containers
// ---------------------------------------------------------------------------

TEST_CASE("UserName: case-variant names collapse to one slot in unordered_set",
          "[domain][shared][user_name]") {
    std::unordered_set<UserName> s;
    s.insert(UserName::parse("Alice").value());
    s.insert(UserName::parse("alice").value());
    s.insert(UserName::parse("ALICE").value());
    REQUIRE(s.size() == 1u);
}

TEST_CASE("UserName: distinct names occupy distinct slots in unordered_set",
          "[domain][shared][user_name]") {
    std::unordered_set<UserName> s;
    s.insert(UserName::parse("Alice").value());
    s.insert(UserName::parse("Bob").value());
    REQUIRE(s.size() == 2u);
}

TEST_CASE("UserName: hash of unequal names need not match but equal names must",
          "[domain][shared][user_name]") {
    std::hash<UserName> h;
    auto a = UserName::parse("SameName").value();
    auto b = UserName::parse("samename").value();
    REQUIRE(h(a) == h(b));
}
