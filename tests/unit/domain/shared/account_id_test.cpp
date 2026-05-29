// SPDX-License-Identifier: GPL-2.0-or-later

/// @file account_id_test.cpp
/// Unit tests for domain strong-typed IDs: AccountId, ChannelId, GameId,
/// ClanId, TeamId, SessionId, ConnectionId.
///
/// StrongId uses C++20 `operator<=>` (defaulted), which synthesises ==, !=,
/// <, <=, >, >= from the spaceship operator.

#include <catch2/catch_test_macros.hpp>
#include <functional>

#include "domain/shared/ids.hpp"

using namespace pvpgn::domain;

// ---------------------------------------------------------------------------
// AccountId
// ---------------------------------------------------------------------------

TEST_CASE("AccountId: construction and value access",
          "[domain][shared][ids][account_id]") {
    AccountId id{42};
    REQUIRE(id.value() == 42u);
}

TEST_CASE("AccountId: is_valid — zero is invalid, non-zero is valid",
          "[domain][shared][ids][account_id]") {
    REQUIRE(AccountId{1}.is_valid());
    REQUIRE(AccountId{0xFFFFFFFFu}.is_valid());
    REQUIRE_FALSE(AccountId{0}.is_valid());
}

TEST_CASE("AccountId: equality via value comparison",
          "[domain][shared][ids][account_id]") {
    AccountId a{1}, b{1}, c{2};
    REQUIRE(a.value() == b.value());
    REQUIRE(a.value() != c.value());
}

TEST_CASE("AccountId: ordering via value comparison",
          "[domain][shared][ids][account_id]") {
    AccountId a{1}, b{2};
    REQUIRE(a.value() < b.value());
    REQUIRE(b.value() > a.value());
}

// ---------------------------------------------------------------------------
// ChannelId
// ---------------------------------------------------------------------------

TEST_CASE("ChannelId: construction and value access",
          "[domain][shared][ids][channel_id]") {
    ChannelId id{7};
    REQUIRE(id.value() == 7u);
    REQUIRE(id.is_valid());
    REQUIRE_FALSE(ChannelId{0}.is_valid());
}

TEST_CASE("ChannelId: equality via value",
          "[domain][shared][ids][channel_id]") {
    REQUIRE(ChannelId{3}.value() == ChannelId{3}.value());
    REQUIRE(ChannelId{3}.value() != ChannelId{4}.value());
}

// ---------------------------------------------------------------------------
// GameId
// ---------------------------------------------------------------------------

TEST_CASE("GameId: construction and validity",
          "[domain][shared][ids][game_id]") {
    REQUIRE(GameId{1}.is_valid());
    REQUIRE_FALSE(GameId{0}.is_valid());
    REQUIRE(GameId{100}.value() == 100u);
}

// ---------------------------------------------------------------------------
// ClanId
// ---------------------------------------------------------------------------

TEST_CASE("ClanId: construction and validity",
          "[domain][shared][ids][clan_id]") {
    REQUIRE(ClanId{5}.is_valid());
    REQUIRE_FALSE(ClanId{0}.is_valid());
    REQUIRE(ClanId{5}.value() == 5u);
}

// ---------------------------------------------------------------------------
// TeamId
// ---------------------------------------------------------------------------

TEST_CASE("TeamId: construction and validity",
          "[domain][shared][ids][team_id]") {
    REQUIRE(TeamId{99}.is_valid());
    REQUIRE_FALSE(TeamId{0}.is_valid());
    REQUIRE(TeamId{99}.value() == 99u);
}

// ---------------------------------------------------------------------------
// SessionId (uint64_t)
// ---------------------------------------------------------------------------

TEST_CASE("SessionId: construction with large value",
          "[domain][shared][ids][session_id]") {
    SessionId id{0xDEADBEEFCAFEBABEull};
    REQUIRE(id.value() == 0xDEADBEEFCAFEBABEull);
    REQUIRE(id.is_valid());
    REQUIRE_FALSE(SessionId{0}.is_valid());
}

TEST_CASE("SessionId: equality via value",
          "[domain][shared][ids][session_id]") {
    REQUIRE(SessionId{1}.value() == SessionId{1}.value());
    REQUIRE(SessionId{1}.value() != SessionId{2}.value());
}

// ---------------------------------------------------------------------------
// ConnectionId
// ---------------------------------------------------------------------------

TEST_CASE("ConnectionId: construction and validity",
          "[domain][shared][ids][connection_id]") {
    ConnectionId id{1};
    REQUIRE(id.value() == 1u);
    REQUIRE(id.is_valid());
    REQUIRE_FALSE(ConnectionId{0}.is_valid());
}

TEST_CASE("ConnectionId: equality via value",
          "[domain][shared][ids][connection_id]") {
    ConnectionId a{1}, b{2}, c{1};
    REQUIRE(a.value() == c.value());
    REQUIRE(a.value() != b.value());
}

// ---------------------------------------------------------------------------
// Type safety — different ID types must not compare equal
// ---------------------------------------------------------------------------

TEST_CASE("AccountId and ChannelId are distinct types",
          "[domain][shared][ids]") {
    // Compile-time check: AccountId{1} == ChannelId{1} must NOT compile.
    // We verify the types are distinct via type_info at runtime.
    AccountId a{1};
    ChannelId c{1};
    REQUIRE(typeid(a) != typeid(c));
    // Both hold the same underlying value but are different types.
    REQUIRE(a.value() == c.value());
}

// ---------------------------------------------------------------------------
// std::hash support
// ---------------------------------------------------------------------------

TEST_CASE("AccountId: std::hash is consistent for equal values",
          "[domain][shared][ids]") {
    std::hash<AccountId> h;
    REQUIRE(h(AccountId{42}) == h(AccountId{42}));
}

TEST_CASE("AccountId: std::hash differs for different values",
          "[domain][shared][ids]") {
    std::hash<AccountId> h;
    // Different values should (almost certainly) hash differently.
    REQUIRE(h(AccountId{1}) != h(AccountId{2}));
}
