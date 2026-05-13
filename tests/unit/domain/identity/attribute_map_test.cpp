// SPDX-License-Identifier: GPL-2.0-or-later
#include <variant>

#include <catch2/catch_test_macros.hpp>

#include "domain/identity/attribute_map.hpp"

using namespace pvpgn;
using domain::AccountId;
using domain::identity::AttributeMap;

TEST_CASE("AttributeMap: set/get round-trip + idempotent writes",
          "[domain][identity][attrs]") {
    AttributeMap a{AccountId{42}};
    REQUIRE_FALSE(a.get("BNET\\acct\\username").has_value());

    REQUIRE(a.set("BNET\\acct\\username", "Alice"));
    REQUIRE(std::string{a.get("BNET\\acct\\username").value()} == "Alice");

    // Same value → no event, no change.
    REQUIRE_FALSE(a.set("BNET\\acct\\username", "Alice"));

    auto evs = a.drain_events();
    REQUIRE(evs.size() == 1);
    REQUIRE(std::holds_alternative<domain::events::AccountAttributeChanged>(evs[0]));
}

TEST_CASE("AttributeMap: overwriting an existing key emits a change event",
          "[domain][identity][attrs]") {
    AttributeMap a{AccountId{42}};
    REQUIRE(a.set("k", "v1"));
    (void)a.drain_events();
    REQUIRE(a.set("k", "v2"));
    REQUIRE(std::string{a.get("k").value()} == "v2");
    REQUIRE(a.drain_events().size() == 1);
}

TEST_CASE("AttributeMap::rehydrate restores values silently",
          "[domain][identity][attrs]") {
    std::unordered_map<std::string, std::string> initial = {
        {"k1", "v1"}, {"k2", "v2"}};
    auto a = AttributeMap::rehydrate(AccountId{7}, initial);
    REQUIRE(a.size() == 2);
    REQUIRE(std::string{a.get("k1").value()} == "v1");
    REQUIRE(a.drain_events().empty());
}

TEST_CASE("AttributeMap::erase returns true on hit, false on miss",
          "[domain][identity][attrs]") {
    AttributeMap a{AccountId{1}};
    REQUIRE(a.set("k", "v"));
    REQUIRE(a.erase("k"));
    REQUIRE_FALSE(a.erase("k"));
    REQUIRE_FALSE(a.get("k").has_value());
}
