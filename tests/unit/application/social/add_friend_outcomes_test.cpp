// SPDX-License-Identifier: GPL-2.0-or-later
//
// Additional outcome tests for `application::social::AddFriend`:
// duplicate friend (AlreadyFriend) and full list (FriendsListFull).
// Mirrors add_friend_test.cpp for fakes/helpers.

#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "application/social/add_friend.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"
#include "domain/identity/account.hpp"
#include "domain/social/friend_list.hpp"
#include "infra/inmemory/account_repository.hpp"
#include "infra/inmemory/event_bus.hpp"
#include "infra/inmemory/friend_list_repository.hpp"

namespace {

using namespace pvpgn;
using application::social::AddFriend;
using application::social::AddFriendError;

domain::UserName make_name(std::string_view s) {
    auto r = domain::UserName::parse(s);
    REQUIRE(r);
    return r.value();
}

domain::BNHash make_hash(std::uint8_t fill) {
    domain::BNHash::Bytes b{};
    b.fill(fill);
    return domain::BNHash{b};
}

std::shared_ptr<infra::inmemory::InMemoryAccountRepository>
make_accounts(std::initializer_list<std::pair<domain::AccountId, std::string_view>> who) {
    auto accounts = std::make_shared<infra::inmemory::InMemoryAccountRepository>();
    std::uint8_t fill = 0xA0;
    for (auto& [id, name] : who) {
        auto a = domain::identity::Account::create(
            id, make_name(name), make_hash(fill++), domain::Locale{}).value();
        (void)a.drain_events();
        REQUIRE(accounts->save(a));
    }
    return accounts;
}

}  // namespace

TEST_CASE("AddFriend: adding an existing friend returns AlreadyFriend",
          "[application][social][add_friend]") {
    domain::AccountId alice{1};
    domain::AccountId bob{2};
    auto accounts = make_accounts({{alice, "Alice"}, {bob, "Bobby"}});

    auto friend_lists =
        std::make_shared<infra::inmemory::InMemoryFriendListRepository>();
    auto event_bus = std::make_shared<infra::inmemory::InMemoryEventBus>();

    AddFriend uc{accounts, friend_lists, event_bus};

    // First add succeeds.
    REQUIRE(uc.execute(alice, bob));

    // Second add of the same target is a duplicate.
    auto r = uc.execute(alice, bob);
    REQUIRE_FALSE(r);
    REQUIRE(r.error() == AddFriendError::AlreadyFriend);
}

TEST_CASE("AddFriend: adding to a full list returns FriendsListFull",
          "[application][social][add_friend]") {
    domain::AccountId owner{1};
    domain::AccountId newcomer{1000};

    auto accounts = make_accounts({{owner, "Owner"}, {newcomer, "Newcomer"}});

    // Pre-seed a friend list already at the legacy cap of kMaxFriends.
    std::vector<domain::AccountId> friends;
    for (std::uint32_t i = 0; i < domain::social::FriendList::kMaxFriends; ++i) {
        friends.push_back(domain::AccountId{100 + i});
    }
    auto full_list = domain::social::FriendList::rehydrate(owner, friends);
    REQUIRE(full_list.size() == domain::social::FriendList::kMaxFriends);

    auto friend_lists =
        std::make_shared<infra::inmemory::InMemoryFriendListRepository>();
    REQUIRE(friend_lists->save(full_list));

    auto event_bus = std::make_shared<infra::inmemory::InMemoryEventBus>();

    AddFriend uc{accounts, friend_lists, event_bus};
    auto r = uc.execute(owner, newcomer);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == AddFriendError::FriendsListFull);
}
