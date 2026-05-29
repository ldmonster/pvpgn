// SPDX-License-Identifier: GPL-2.0-or-later

/// @file user_name_test.cpp
/// Unit tests for domain::UserName value object.

#include <catch2/catch_test_macros.hpp>
#include <sstream>

#include "domain/shared/user_name.hpp"

using pvpgn::domain::UserName;

// ---------------------------------------------------------------------------
// Construction from valid input
// ---------------------------------------------------------------------------

TEST_CASE("UserName: parse accepts minimum-length name (2 chars)",
          "[domain][shared][user_name]") {
    auto result = UserName::parse("Ab");
    REQUIRE(result.has_value());
    REQUIRE(std::string{result.value().display()} == "Ab");
}

TEST_CASE("UserName: parse accepts maximum-length name (15 chars)",
          "[domain][shared][user_name]") {
    auto result = UserName::parse("Abcdefghijklmno");  // 15 chars
    REQUIRE(result.has_value());
    REQUIRE(result.value().display().size() == 15u);
}

TEST_CASE("UserName: parse accepts letters, digits, underscore, dash, dot",
          "[domain][shared][user_name]") {
    REQUIRE(UserName::parse("Alice").has_value());
    REQUIRE(UserName::parse("Bob123").has_value());
    REQUIRE(UserName::parse("User_Name").has_value());
    REQUIRE(UserName::parse("User-Name").has_value());
    REQUIRE(UserName::parse("User.Name").has_value());
}

// ---------------------------------------------------------------------------
// Rejection of invalid input
// ---------------------------------------------------------------------------

TEST_CASE("UserName: parse rejects empty string",
          "[domain][shared][user_name]") {
    REQUIRE_FALSE(UserName::parse("").has_value());
}

TEST_CASE("UserName: parse rejects single-character name (too short)",
          "[domain][shared][user_name]") {
    REQUIRE_FALSE(UserName::parse("A").has_value());
}

TEST_CASE("UserName: parse rejects 16-character name (too long)",
          "[domain][shared][user_name]") {
    REQUIRE_FALSE(UserName::parse("Abcdefghijklmnop").has_value());  // 16 chars
}

TEST_CASE("UserName: parse rejects name starting with digit",
          "[domain][shared][user_name]") {
    REQUIRE_FALSE(UserName::parse("1Alice").has_value());
}

TEST_CASE("UserName: parse rejects name starting with underscore",
          "[domain][shared][user_name]") {
    REQUIRE_FALSE(UserName::parse("_Alice").has_value());
}

TEST_CASE("UserName: parse rejects name with space",
          "[domain][shared][user_name]") {
    REQUIRE_FALSE(UserName::parse("Al ice").has_value());
}

TEST_CASE("UserName: parse rejects name with control character",
          "[domain][shared][user_name]") {
    REQUIRE_FALSE(UserName::parse("Al\x01ice").has_value());
}

TEST_CASE("UserName: parse rejects name with at-sign",
          "[domain][shared][user_name]") {
    REQUIRE_FALSE(UserName::parse("Al@ice").has_value());
}

// ---------------------------------------------------------------------------
// Equality comparison (case-insensitive canonical form)
// ---------------------------------------------------------------------------

TEST_CASE("UserName: equality is case-insensitive",
          "[domain][shared][user_name]") {
    auto a = UserName::parse("Alice").value();
    auto b = UserName::parse("alice").value();
    auto c = UserName::parse("ALICE").value();
    REQUIRE(a == b);
    REQUIRE(a == c);
    REQUIRE(b == c);
}

TEST_CASE("UserName: inequality for different names",
          "[domain][shared][user_name]") {
    auto a = UserName::parse("Alice").value();
    auto b = UserName::parse("Bob").value();
    REQUIRE(a != b);
}

// ---------------------------------------------------------------------------
// display() vs canonical()
// ---------------------------------------------------------------------------

TEST_CASE("UserName: display preserves original casing",
          "[domain][shared][user_name]") {
    auto u = UserName::parse("AlIcE").value();
    REQUIRE(std::string{u.display()} == "AlIcE");
}

TEST_CASE("UserName: canonical is lower-case",
          "[domain][shared][user_name]") {
    auto u = UserName::parse("AlIcE").value();
    REQUIRE(std::string{u.canonical()} == "alice");
}

// ---------------------------------------------------------------------------
// std::hash support
// ---------------------------------------------------------------------------

TEST_CASE("UserName: std::hash is consistent for equal names",
          "[domain][shared][user_name]") {
    auto a = UserName::parse("Alice").value();
    auto b = UserName::parse("alice").value();
    std::hash<UserName> h;
    REQUIRE(h(a) == h(b));
}
