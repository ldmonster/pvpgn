// SPDX-License-Identifier: GPL-2.0-or-later

/// @file attribute_map_typed_test.cpp
/// Unit tests for `AttributeMap` typed accessors.

#include <catch2/catch_test_macros.hpp>

#include "domain/identity/attribute_map.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::domain::identity {

using AccountId = pvpgn::domain::AccountId;
using ClientTag = pvpgn::domain::ClientTag;

// --- Identity attribute tests ---

TEST_CASE("AttributeMap/SetAndGetEmail", "[domain][identity]") {
    AccountId owner_{1};
    AttributeMap map{owner_};
    map.set_email("test@example.com");
    
    auto email = map.get("BNET\\acct\\email");
    CHECK(email.has_value());
    CHECK(std::string{email.value()} == "test@example.com");
}

TEST_CASE("AttributeMap/SetAndGetSex", "[domain][identity]") {
    AccountId owner_{1};
    AttributeMap map{owner_};
    map.set_sex("M");
    
    auto sex = map.get("BNET\\acct\\sex");
    CHECK(sex.has_value());
    CHECK(std::string{sex.value()} == "M");
}

TEST_CASE("AttributeMap/SetAndGetLocation", "[domain][identity]") {
    AccountId owner_{1};
    AttributeMap map{owner_};
    map.set_location("USA");
    
    auto location = map.get("BNET\\acct\\location");
    CHECK(location.has_value());
    CHECK(std::string{location.value()} == "USA");
}

TEST_CASE("AttributeMap/SetAndGetDescription", "[domain][identity]") {
    AccountId owner_{1};
    AttributeMap map{owner_};
    map.set_description("A cool player");
    
    auto desc = map.get("BNET\\acct\\description");
    CHECK(desc.has_value());
    CHECK(std::string{desc.value()} == "A cool player");
}

// --- Game statistics tests ---

TEST_CASE("AttributeMap/WinsDefaultZero", "[domain][identity]") {
    AccountId owner_{1};
    AttributeMap map{owner_};
    auto star = ClientTag::parse("STAR").value();
    
    auto wins = map.wins(star);
    CHECK(wins == 0);
}

TEST_CASE("AttributeMap/IncrementWins", "[domain][identity]") {
    AccountId owner_{1};
    AttributeMap map{owner_};
    auto star = ClientTag::parse("STAR").value();
    
    map.increment_wins(star);
    map.increment_wins(star);
    
    CHECK(map.wins(star) == 2);
}

TEST_CASE("AttributeMap/IncrementLosses", "[domain][identity]") {
    AccountId owner_{1};
    AttributeMap map{owner_};
    auto d2dv = ClientTag::parse("D2DV").value();
    
    map.increment_losses(d2dv);
    
    CHECK(map.losses(d2dv) == 1);
}

TEST_CASE("AttributeMap/IncrementDisconnects", "[domain][identity]") {
    AccountId owner_{1};
    AttributeMap map{owner_};
    auto war3 = ClientTag::parse("WAR3").value();
    
    map.increment_disconnects(war3);
    map.increment_disconnects(war3);
    map.increment_disconnects(war3);
    
    CHECK(map.disconnects(war3) == 3);
}

TEST_CASE("AttributeMap/IncrementLadderWins", "[domain][identity]") {
    AccountId owner_{1};
    AttributeMap map{owner_};
    auto star = ClientTag::parse("STAR").value();
    
    map.increment_ladder_wins(star);
    
    CHECK(map.ladder_wins(star) == 1);
}

TEST_CASE("AttributeMap/IncrementLadderLosses", "[domain][identity]") {
    AccountId owner_{1};
    AttributeMap map{owner_};
    auto war3 = ClientTag::parse("WAR3").value();
    
    map.increment_ladder_losses(war3);
    map.increment_ladder_losses(war3);
    
    CHECK(map.ladder_losses(war3) == 2);
}

TEST_CASE("AttributeMap/MultipleClientTags", "[domain][identity]") {
    AccountId owner_{1};
    AttributeMap map{owner_};
    auto star = ClientTag::parse("STAR").value();
    auto d2dv = ClientTag::parse("D2DV").value();
    
    map.increment_wins(star);
    map.increment_wins(star);
    map.increment_wins(d2dv);
    
    CHECK(map.wins(star) == 2);
    CHECK(map.wins(d2dv) == 1);
}

TEST_CASE("AttributeMap/EventsEmittedOnSet", "[domain][identity]") {
    AccountId owner_{1};
    AttributeMap map{owner_};
    
    map.set_email("user@example.com");
    map.set_location("US");
    
    auto events = map.drain_events();
    CHECK(events.size() == 2);
}

TEST_CASE("AttributeMap/EventsEmittedOnIncrement", "[domain][identity]") {
    AccountId owner_{1};
    AttributeMap map{owner_};
    auto star = ClientTag::parse("STAR").value();
    
    map.increment_wins(star);
    map.increment_losses(star);
    
    auto events = map.drain_events();
    CHECK(events.size() == 2);
}

}  // namespace pvpgn::domain::identity
