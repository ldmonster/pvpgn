// SPDX-License-Identifier: GPL-2.0-or-later

/// @file whisper_test.cpp
/// Unit tests for `chat::IgnoreList` aggregate.

#include <catch2/catch_test_macros.hpp>

#include "domain/chat/whisper.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::domain::chat {

using AccountId = pvpgn::domain::AccountId;

TEST_CASE("IgnoreList/CreateEmpty", "[domain][chat]") {
    AccountId owner_{1};
    AccountId alice_{2};
    
    auto ignore_list = IgnoreList::create(owner_);
    CHECK(ignore_list.is_empty());
    CHECK(ignore_list.size() == 0);
    CHECK_FALSE(ignore_list.ignores(alice_));
}

TEST_CASE("IgnoreList/AddSingleTarget", "[domain][chat]") {
    AccountId owner_{1};
    AccountId alice_{2};
    AccountId bob_{3};
    
    auto ignore_list = IgnoreList::create(owner_);
    bool added = ignore_list.add(alice_);
    
    CHECK(added);
    CHECK_FALSE(ignore_list.is_empty());
    CHECK(ignore_list.size() == 1);
    CHECK(ignore_list.ignores(alice_));
    CHECK_FALSE(ignore_list.ignores(bob_));
    
    auto events = ignore_list.drain_events();
    CHECK(events.size() == 1);
}

TEST_CASE("IgnoreList/AddDuplicate", "[domain][chat]") {
    AccountId owner_{1};
    AccountId alice_{2};
    
    auto ignore_list = IgnoreList::create(owner_);
    bool first_add = ignore_list.add(alice_);
    bool second_add = ignore_list.add(alice_);
    
    CHECK(first_add);
    CHECK_FALSE(second_add);  // No-op, no event
    CHECK(ignore_list.size() == 1);
    
    auto events = ignore_list.drain_events();
    CHECK(events.size() == 1);  // Only one IgnoreAdded event
}

TEST_CASE("IgnoreList/AddMultiple", "[domain][chat]") {
    AccountId owner_{1};
    AccountId alice_{2};
    AccountId bob_{3};
    AccountId charlie_{4};
    
    auto ignore_list = IgnoreList::create(owner_);
    ignore_list.add(alice_);
    ignore_list.add(bob_);
    ignore_list.add(charlie_);
    
    CHECK(ignore_list.size() == 3);
    CHECK(ignore_list.ignores(alice_));
    CHECK(ignore_list.ignores(bob_));
    CHECK(ignore_list.ignores(charlie_));
    
    auto events = ignore_list.drain_events();
    CHECK(events.size() == 3);
}

TEST_CASE("IgnoreList/RemoveExisting", "[domain][chat]") {
    AccountId owner_{1};
    AccountId alice_{2};
    AccountId bob_{3};
    
    auto ignore_list = IgnoreList::create(owner_);
    ignore_list.add(alice_);
    ignore_list.add(bob_);
    
    bool removed = ignore_list.remove(alice_);
    
    CHECK(removed);
    CHECK(ignore_list.size() == 1);
    CHECK_FALSE(ignore_list.ignores(alice_));
    CHECK(ignore_list.ignores(bob_));
    
    auto events = ignore_list.drain_events();
    CHECK(events.size() == 3);  // 2 adds + 1 remove
}

TEST_CASE("IgnoreList/RemoveNonExisting", "[domain][chat]") {
    AccountId owner_{1};
    AccountId alice_{2};
    AccountId bob_{3};
    
    auto ignore_list = IgnoreList::create(owner_);
    ignore_list.add(alice_);
    
    bool removed = ignore_list.remove(bob_);
    
    CHECK_FALSE(removed);
    CHECK(ignore_list.size() == 1);
    CHECK(ignore_list.ignores(alice_));
    
    auto events = ignore_list.drain_events();
    CHECK(events.size() == 1);  // Only the add event
}

TEST_CASE("IgnoreList/Rehydrate", "[domain][chat]") {
    AccountId owner_{1};
    AccountId alice_{2};
    AccountId bob_{3};
    AccountId charlie_{4};
    
    std::vector<AccountId> ignored{alice_, bob_};
    auto ignore_list = IgnoreList::rehydrate(owner_, ignored);
    
    CHECK(ignore_list.size() == 2);
    CHECK(ignore_list.ignores(alice_));
    CHECK(ignore_list.ignores(bob_));
    CHECK_FALSE(ignore_list.ignores(charlie_));
    
    // Rehydrated aggregates emit no events
    auto events = ignore_list.drain_events();
    CHECK(events.size() == 0);
}

TEST_CASE("IgnoreList/DrainEventsTwice", "[domain][chat]") {
    AccountId owner_{1};
    AccountId alice_{2};
    
    auto ignore_list = IgnoreList::create(owner_);
    ignore_list.add(alice_);
    
    auto events1 = ignore_list.drain_events();
    CHECK(events1.size() == 1);
    
    auto events2 = ignore_list.drain_events();
    CHECK(events2.size() == 0);
}

}  // namespace pvpgn::domain::chat
