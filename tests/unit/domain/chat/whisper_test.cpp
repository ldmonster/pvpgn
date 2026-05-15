// SPDX-License-Identifier: GPL-2.0-or-later

/// @file whisper_test.cpp
/// Unit tests for `chat::IgnoreList` aggregate.

#include <gtest/gtest.h>

#include "domain/chat/whisper.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::domain::chat {

using AccountId = pvpgn::domain::AccountId;

class IgnoreListTest : public ::testing::Test {
protected:
    AccountId owner_{1};
    AccountId alice_{2};
    AccountId bob_{3};
    AccountId charlie_{4};
};

TEST_F(IgnoreListTest, CreateEmpty) {
    auto ignore_list = IgnoreList::create(owner_);
    EXPECT_TRUE(ignore_list.is_empty());
    EXPECT_EQ(ignore_list.size(), 0);
    EXPECT_FALSE(ignore_list.ignores(alice_));
}

TEST_F(IgnoreListTest, AddSingleTarget) {
    auto ignore_list = IgnoreList::create(owner_);
    bool added = ignore_list.add(alice_);
    
    EXPECT_TRUE(added);
    EXPECT_FALSE(ignore_list.is_empty());
    EXPECT_EQ(ignore_list.size(), 1);
    EXPECT_TRUE(ignore_list.ignores(alice_));
    EXPECT_FALSE(ignore_list.ignores(bob_));
    
    auto events = ignore_list.drain_events();
    EXPECT_EQ(events.size(), 1);
}

TEST_F(IgnoreListTest, AddDuplicate) {
    auto ignore_list = IgnoreList::create(owner_);
    bool first_add = ignore_list.add(alice_);
    bool second_add = ignore_list.add(alice_);
    
    EXPECT_TRUE(first_add);
    EXPECT_FALSE(second_add);  // No-op, no event
    EXPECT_EQ(ignore_list.size(), 1);
    
    auto events = ignore_list.drain_events();
    EXPECT_EQ(events.size(), 1);  // Only one IgnoreAdded event
}

TEST_F(IgnoreListTest, AddMultiple) {
    auto ignore_list = IgnoreList::create(owner_);
    ignore_list.add(alice_);
    ignore_list.add(bob_);
    ignore_list.add(charlie_);
    
    EXPECT_EQ(ignore_list.size(), 3);
    EXPECT_TRUE(ignore_list.ignores(alice_));
    EXPECT_TRUE(ignore_list.ignores(bob_));
    EXPECT_TRUE(ignore_list.ignores(charlie_));
    
    auto events = ignore_list.drain_events();
    EXPECT_EQ(events.size(), 3);
}

TEST_F(IgnoreListTest, RemoveExisting) {
    auto ignore_list = IgnoreList::create(owner_);
    ignore_list.add(alice_);
    ignore_list.add(bob_);
    
    bool removed = ignore_list.remove(alice_);
    
    EXPECT_TRUE(removed);
    EXPECT_EQ(ignore_list.size(), 1);
    EXPECT_FALSE(ignore_list.ignores(alice_));
    EXPECT_TRUE(ignore_list.ignores(bob_));
    
    auto events = ignore_list.drain_events();
    EXPECT_EQ(events.size(), 3);  // 2 adds + 1 remove
}

TEST_F(IgnoreListTest, RemoveNonExisting) {
    auto ignore_list = IgnoreList::create(owner_);
    ignore_list.add(alice_);
    
    bool removed = ignore_list.remove(bob_);
    
    EXPECT_FALSE(removed);
    EXPECT_EQ(ignore_list.size(), 1);
    EXPECT_TRUE(ignore_list.ignores(alice_));
    
    auto events = ignore_list.drain_events();
    EXPECT_EQ(events.size(), 1);  // Only the add event
}

TEST_F(IgnoreListTest, Rehydrate) {
    std::vector<AccountId> ignored{alice_, bob_};
    auto ignore_list = IgnoreList::rehydrate(owner_, ignored);
    
    EXPECT_EQ(ignore_list.size(), 2);
    EXPECT_TRUE(ignore_list.ignores(alice_));
    EXPECT_TRUE(ignore_list.ignores(bob_));
    EXPECT_FALSE(ignore_list.ignores(charlie_));
    
    // Rehydrated aggregates emit no events
    auto events = ignore_list.drain_events();
    EXPECT_EQ(events.size(), 0);
}

TEST_F(IgnoreListTest, DrainEventsTwice) {
    auto ignore_list = IgnoreList::create(owner_);
    ignore_list.add(alice_);
    
    auto events1 = ignore_list.drain_events();
    EXPECT_EQ(events1.size(), 1);
    
    auto events2 = ignore_list.drain_events();
    EXPECT_EQ(events2.size(), 0);
}

}  // namespace pvpgn::domain::chat
