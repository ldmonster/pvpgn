// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::social::RemoveFriend`.

#include <catch2/catch_test_macros.hpp>

#include "application/social/remove_friend.hpp"
#include "domain/shared/ids.hpp"
#include "domain/social/friend_list.hpp"
#include "infra/inmemory/event_bus.hpp"
#include "infra/inmemory/friend_list_repository.hpp"

namespace {

using namespace pvpgn;
using application::social::RemoveFriend;
using application::social::RemoveFriendError;

struct Fixture {
    infra::inmemory::InMemoryFriendListRepository friend_lists;
    infra::inmemory::InMemoryEventBus             bus;

    domain::AccountId alice{1};
    domain::AccountId bob{2};

    void seed_friendship() {
        domain::social::FriendList list{alice};
        list.add(bob);
        (void)list.drain_events();
        REQUIRE(friend_lists.save(list));
    }

    RemoveFriend make_uc() {
        return RemoveFriend{
            std::make_shared<infra::inmemory::InMemoryFriendListRepository>(friend_lists),
            std::make_shared<infra::inmemory::InMemoryEventBus>()};
    }
};

}  // namespace

TEST_CASE("RemoveFriend: happy path removes an existing friend",
          "[application][social][remove_friend]") {
    Fixture f;
    f.seed_friendship();
    auto uc = f.make_uc();

    auto r = uc.execute(f.alice, f.bob);

    REQUIRE(r);
}

TEST_CASE("RemoveFriend: removing non-friend returns NotAFriend",
          "[application][social][remove_friend]") {
    Fixture f;
    // No friendship seeded — alice has an empty list
    auto uc = f.make_uc();

    auto r = uc.execute(f.alice, f.bob);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == RemoveFriendError::NotAFriend);
}

TEST_CASE("RemoveFriend: removing self is also NotAFriend",
          "[application][social][remove_friend]") {
    Fixture f;
    auto uc = f.make_uc();

    auto r = uc.execute(f.alice, f.alice);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == RemoveFriendError::NotAFriend);
}
