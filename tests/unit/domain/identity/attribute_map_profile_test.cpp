// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for AttributeMap accessors not reached by the existing
// attribute_map_test / _typed / _timestamps suites: the profile typed
// getters+setters (email/sex/location/description) round-trips, owner()/
// values(), and the wins/losses/disconnects stat increments + per-tag
// isolation.

#include <catch2/catch_test_macros.hpp>

#include "domain/identity/attribute_map.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"

using pvpgn::domain::AccountId;
using pvpgn::domain::ClientTag;
using pvpgn::domain::identity::AttributeMap;

namespace {

AttributeMap fresh(std::uint32_t id = 1) { return AttributeMap{AccountId{id}}; }

}  // namespace

TEST_CASE("AttributeMap: profile setters round-trip through their typed getters",
          "[domain][identity][attrs]") {
    auto map = fresh();
    CHECK_FALSE(map.email().has_value());
    CHECK_FALSE(map.sex().has_value());
    CHECK_FALSE(map.location().has_value());
    CHECK_FALSE(map.description().has_value());

    map.set_email("a@b.c");
    map.set_sex("F");
    map.set_location("Betelgeuse");
    map.set_description("hoopy frood");

    REQUIRE(map.email().has_value());
    CHECK(map.email().value() == "a@b.c");
    REQUIRE(map.sex().has_value());
    CHECK(map.sex().value() == "F");
    REQUIRE(map.location().has_value());
    CHECK(map.location().value() == "Betelgeuse");
    REQUIRE(map.description().has_value());
    CHECK(map.description().value() == "hoopy frood");
}

TEST_CASE("AttributeMap: profile setters write the verbatim BNET keys",
          "[domain][identity][attrs]") {
    auto map = fresh();
    map.set_email("e@x");
    map.set_location("L");
    CHECK(std::string{map.get("BNET\\acct\\email").value()} == "e@x");
    CHECK(std::string{map.get("BNET\\acct\\location").value()} == "L");
}

TEST_CASE("AttributeMap: owner() and values() expose construction state",
          "[domain][identity][attrs]") {
    auto map = fresh(77);
    CHECK(map.owner() == AccountId{77});
    CHECK(map.values().empty());

    map.set("k", "v");
    CHECK(map.size() == 1);
    CHECK(map.values().size() == 1);
    CHECK(map.values().at("k") == "v");
}

TEST_CASE("AttributeMap: wins/losses/disconnects increments accumulate per stat",
          "[domain][identity][attrs]") {
    auto map = fresh();
    const auto star = ClientTag::parse("STAR").value();

    CHECK(map.wins(star) == 0);
    CHECK(map.losses(star) == 0);
    CHECK(map.disconnects(star) == 0);

    map.increment_wins(star);
    map.increment_wins(star);
    map.increment_losses(star);
    map.increment_disconnects(star);
    map.increment_disconnects(star);
    map.increment_disconnects(star);

    CHECK(map.wins(star) == 2);
    CHECK(map.losses(star) == 1);
    CHECK(map.disconnects(star) == 3);
}

TEST_CASE("AttributeMap: stats are isolated per ClientTag",
          "[domain][identity][attrs]") {
    auto map = fresh();
    const auto star = ClientTag::parse("STAR").value();
    const auto war3 = ClientTag::parse("WAR3").value();

    map.increment_wins(star);
    map.increment_wins(star);
    map.increment_wins(war3);

    CHECK(map.wins(star) == 2);
    CHECK(map.wins(war3) == 1);
}
