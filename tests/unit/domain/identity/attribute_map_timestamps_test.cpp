// SPDX-License-Identifier: GPL-2.0-or-later
//
// Focused tests for the AttributeMap accessors the existing typed test does not
// reach: username(), last_login()/created_at() (parse + round-trip + the
// malformed-value catch paths), and the stat getters' non-numeric fallbacks.

#include <catch2/catch_test_macros.hpp>

#include <chrono>

#include "domain/identity/attribute_map.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"

namespace {

using pvpgn::domain::AccountId;
using pvpgn::domain::ClientTag;
using pvpgn::domain::identity::AttributeMap;

AttributeMap fresh() { return AttributeMap{AccountId{1}}; }

}  // namespace

TEST_CASE("AttributeMap: username() reads the raw BNET key", "[domain][identity]") {
    auto map = fresh();
    CHECK_FALSE(map.username().has_value());

    map.set("BNET\\acct\\username", "Zaphod");
    REQUIRE(map.username().has_value());
    CHECK(map.username().value() == "Zaphod");
}

TEST_CASE("AttributeMap: last_login round-trips through set_last_login",
          "[domain][identity]") {
    auto map = fresh();
    CHECK_FALSE(map.last_login().has_value());

    const auto when = pvpgn::core::SystemTime{std::chrono::seconds{1'700'000'000}};
    map.set_last_login(when);

    REQUIRE(map.last_login().has_value());
    CHECK(std::chrono::duration_cast<std::chrono::seconds>(
              map.last_login().value().time_since_epoch())
              .count() == 1'700'000'000);
}

TEST_CASE("AttributeMap: created_at round-trips through set_created_at",
          "[domain][identity]") {
    auto map = fresh();
    CHECK_FALSE(map.created_at().has_value());

    const auto when = pvpgn::core::SystemTime{std::chrono::seconds{1'234'567'890}};
    map.set_created_at(when);

    REQUIRE(map.created_at().has_value());
    CHECK(std::chrono::duration_cast<std::chrono::seconds>(
              map.created_at().value().time_since_epoch())
              .count() == 1'234'567'890);
}

TEST_CASE("AttributeMap: malformed timestamps yield nullopt (catch path)",
          "[domain][identity]") {
    auto map = fresh();
    map.set("BNET\\acct\\lastlogin", "not-a-number");
    map.set("BNET\\acct\\createtime", "");

    CHECK_FALSE(map.last_login().has_value());
    CHECK_FALSE(map.created_at().has_value());
}

TEST_CASE("AttributeMap: stat getters fall back to 0 on non-numeric values",
          "[domain][identity]") {
    auto map = fresh();
    const auto star = ClientTag::parse("STAR").value();

    // Seed each Record\\<stat>\\<tag> key with garbage; getters must not throw
    // and must return 0 (the catch branch).
    map.set("Record\\wins\\" + std::to_string(star.packed_be()), "xx");
    map.set("Record\\losses\\" + std::to_string(star.packed_be()), "");
    map.set("Record\\disconnects\\" + std::to_string(star.packed_be()), "?!");
    map.set("Record\\ladder_wins\\" + std::to_string(star.packed_be()), "n/a");
    map.set("Record\\ladder_losses\\" + std::to_string(star.packed_be()), "-");

    CHECK(map.wins(star) == 0);
    CHECK(map.losses(star) == 0);
    CHECK(map.disconnects(star) == 0);
    CHECK(map.ladder_wins(star) == 0);
    CHECK(map.ladder_losses(star) == 0);
}

TEST_CASE("AttributeMap: ladder increments accumulate independently of wins",
          "[domain][identity]") {
    auto map = fresh();
    const auto war3 = ClientTag::parse("WAR3").value();

    map.increment_ladder_wins(war3);
    map.increment_ladder_wins(war3);
    map.increment_ladder_losses(war3);

    CHECK(map.ladder_wins(war3) == 2);
    CHECK(map.ladder_losses(war3) == 1);
    // Non-ladder counters stay zero.
    CHECK(map.wins(war3) == 0);
    CHECK(map.losses(war3) == 0);
}
