// SPDX-License-Identifier: GPL-2.0-or-later
#include <cstdint>
#include <type_traits>
#include <unordered_map>

#include <catch2/catch_test_macros.hpp>

#include "core/strong_typedef.hpp"

using namespace pvpgn::core;

using AccountId = StrongId<struct AccountIdTag, std::uint32_t>;
using ChannelId = StrongId<struct ChannelIdTag, std::uint32_t>;

TEST_CASE("StrongId distinct types", "[core][strong]") {
    static_assert(!std::is_convertible_v<AccountId, ChannelId>);
    static_assert(!std::is_constructible_v<AccountId, ChannelId>);
    SUCCEED();
}

TEST_CASE("StrongId comparison and value", "[core][strong]") {
    AccountId a{7};
    AccountId b{7};
    AccountId c{9};
    REQUIRE(a == b);
    REQUIRE(a != c);
    REQUIRE(a < c);
    REQUIRE(a.value() == 7);
    REQUIRE(a.is_valid());
    REQUIRE_FALSE(AccountId{}.is_valid());
}

TEST_CASE("StrongId hashable as map key", "[core][strong]") {
    std::unordered_map<AccountId, int> m;
    m[AccountId{1}] = 10;
    m[AccountId{2}] = 20;
    REQUIRE(m[AccountId{1}] == 10);
    REQUIRE(m[AccountId{2}] == 20);
}
