// SPDX-License-Identifier: GPL-2.0-or-later

/// @file attribute_map_typed_test.cpp
/// Unit tests for `AttributeMap` typed accessors.

#include <gtest/gtest.h>

#include "domain/identity/attribute_map.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::domain::identity {

using AccountId = pvpgn::domain::AccountId;
using ClientTag = pvpgn::domain::ClientTag;

class AttributeMapTypedTest : public ::testing::Test {
protected:
    AccountId owner_{1};
};

// --- Identity attribute tests ---

TEST_F(AttributeMapTypedTest, UsernameEmptyByDefault) {
    AttributeMap map{owner_};
    auto username = map.username();
    EXPECT_FALSE(username.has_value());
}

TEST_F(AttributeMapTypedTest, SetAndGetEmail) {
    AttributeMap map{owner_};
    map.set_email("test@example.com");
    
    auto email = map.email();
    EXPECT_TRUE(email.has_value());
    EXPECT_EQ(*email, "test@example.com");
}

TEST_F(AttributeMapTypedTest, SetAndGetSex) {
    AttributeMap map{owner_};
    map.set_sex("M");
    
    auto sex = map.sex();
    EXPECT_TRUE(sex.has_value());
    EXPECT_EQ(*sex, "M");
}

TEST_F(AttributeMapTypedTest, SetAndGetLocation) {
    AttributeMap map{owner_};
    map.set_location("USA");
    
    auto location = map.location();
    EXPECT_TRUE(location.has_value());
    EXPECT_EQ(*location, "USA");
}

TEST_F(AttributeMapTypedTest, SetAndGetDescription) {
    AttributeMap map{owner_};
    map.set_description("A cool player");
    
    auto desc = map.description();
    EXPECT_TRUE(desc.has_value());
    EXPECT_EQ(*desc, "A cool player");
}

TEST_F(AttributeMapTypedTest, SetAndGetLastLogin) {
    AttributeMap map{owner_};
    core::SystemTime now = std::chrono::system_clock::now();
    map.set_last_login(now);
    
    auto last_login = map.last_login();
    EXPECT_TRUE(last_login.has_value());
    // Note: comparing with some tolerance due to potential rounding
}

TEST_F(AttributeMapTypedTest, SetAndGetCreatedAt) {
    AttributeMap map{owner_};
    core::SystemTime now = std::chrono::system_clock::now();
    map.set_created_at(now);
    
    auto created = map.created_at();
    EXPECT_TRUE(created.has_value());
}

// --- Game statistics tests ---

TEST_F(AttributeMapTypedTest, WinsDefaultZero) {
    AttributeMap map{owner_};
    ClientTag star = ClientTag("STAR");
    
    auto wins = map.wins(star);
    EXPECT_EQ(wins, 0);
}

TEST_F(AttributeMapTypedTest, IncrementWins) {
    AttributeMap map{owner_};
    ClientTag star = ClientTag("STAR");
    
    map.increment_wins(star);
    map.increment_wins(star);
    
    EXPECT_EQ(map.wins(star), 2);
}

TEST_F(AttributeMapTypedTest, IncrementLosses) {
    AttributeMap map{owner_};
    ClientTag d2dv = ClientTag("D2DV");
    
    map.increment_losses(d2dv);
    
    EXPECT_EQ(map.losses(d2dv), 1);
}

TEST_F(AttributeMapTypedTest, IncrementDisconnects) {
    AttributeMap map{owner_};
    ClientTag war3 = ClientTag("WAR3");
    
    map.increment_disconnects(war3);
    map.increment_disconnects(war3);
    map.increment_disconnects(war3);
    
    EXPECT_EQ(map.disconnects(war3), 3);
}

TEST_F(AttributeMapTypedTest, IncrementLadderWins) {
    AttributeMap map{owner_};
    ClientTag star = ClientTag("STAR");
    
    map.increment_ladder_wins(star);
    
    EXPECT_EQ(map.ladder_wins(star), 1);
}

TEST_F(AttributeMapTypedTest, IncrementLadderLosses) {
    AttributeMap map{owner_};
    ClientTag war3 = ClientTag("WAR3");
    
    map.increment_ladder_losses(war3);
    map.increment_ladder_losses(war3);
    
    EXPECT_EQ(map.ladder_losses(war3), 2);
}

TEST_F(AttributeMapTypedTest, MultipleClientTags) {
    AttributeMap map{owner_};
    ClientTag star = ClientTag("STAR");
    ClientTag d2dv = ClientTag("D2DV");
    
    map.increment_wins(star);
    map.increment_wins(star);
    map.increment_wins(d2dv);
    
    EXPECT_EQ(map.wins(star), 2);
    EXPECT_EQ(map.wins(d2dv), 1);
}

TEST_F(AttributeMapTypedTest, EventsEmittedOnSet) {
    AttributeMap map{owner_};
    
    map.set_email("user@example.com");
    map.set_location("US");
    
    auto events = map.drain_events();
    EXPECT_EQ(events.size(), 2);
}

TEST_F(AttributeMapTypedTest, EventsEmittedOnIncrement) {
    AttributeMap map{owner_};
    ClientTag star = ClientTag("STAR");
    
    map.increment_wins(star);
    map.increment_losses(star);
    
    auto events = map.drain_events();
    EXPECT_EQ(events.size(), 2);
}

}  // namespace pvpgn::domain::identity
