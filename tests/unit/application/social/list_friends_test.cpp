// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::social::ListFriends`.

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "application/social/list_friends.hpp"
#include "domain/identity/ports.hpp"
#include "domain/social/ports.hpp"
#include "domain/identity/ports.hpp"
#include "domain/shared/ids.hpp"
#include "domain/social/friend_list.hpp"
#include "infra/inmemory/event_bus.hpp"
#include "infra/inmemory/friend_list_repository.hpp"
#include "infra/inmemory/account_repository.hpp"
#include "infra/inmemory/session_registry.hpp"

namespace {

using namespace pvpgn;
using application::social::ListFriends;
using application::social::ListFriendsError;

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

}  // namespace

TEST_CASE("ListFriends: returns empty list when owner has no friends",
          "[application][social][list_friends]") {
    infra::inmemory::InMemoryAccountRepository    accounts;
    infra::inmemory::InMemoryFriendListRepository friend_lists;
    infra::inmemory::InMemorySessionRegistry      sessions;

    domain::AccountId alice{1};
    auto a = domain::identity::Account::create(
        alice, make_name("Alice"), make_hash(0xAA), domain::Locale{}).value();
    (void)a.drain_events();
    REQUIRE(accounts.save(a));

    ListFriends uc{
        std::make_shared<infra::inmemory::InMemoryFriendListRepository>(),
        std::make_shared<infra::inmemory::InMemorySessionRegistry>(),
        std::make_shared<infra::inmemory::InMemoryAccountRepository>(accounts)};

    auto r = uc.execute(alice);

    REQUIRE(r);
    REQUIRE(r.value().empty());
}

TEST_CASE("ListFriends: returns friend info for each friend in list",
          "[application][social][list_friends]") {
    infra::inmemory::InMemoryAccountRepository    accounts;
    infra::inmemory::InMemoryFriendListRepository friend_lists;
    infra::inmemory::InMemorySessionRegistry      sessions;

    domain::AccountId alice{1};
    domain::AccountId bob{2};

    auto a = domain::identity::Account::create(
        alice, make_name("Alice"), make_hash(0xAA), domain::Locale{}).value();
    (void)a.drain_events();
    REQUIRE(accounts.save(a));

    auto b = domain::identity::Account::create(
        bob, make_name("Bobby"), make_hash(0xBB), domain::Locale{}).value();
    (void)b.drain_events();
    REQUIRE(accounts.save(b));

    // Seed alice's friend list with bob
    domain::social::FriendList list{alice};
    list.add(bob);
    (void)list.drain_events();
    REQUIRE(friend_lists.save(list));

    ListFriends uc{
        std::make_shared<infra::inmemory::InMemoryFriendListRepository>(friend_lists),
        std::make_shared<infra::inmemory::InMemorySessionRegistry>(),
        std::make_shared<infra::inmemory::InMemoryAccountRepository>(accounts)};

    auto r = uc.execute(alice);

    REQUIRE(r);
    REQUIRE(r.value().size() == 1);
    REQUIRE(r.value()[0].id.value() == bob.value());
    REQUIRE_FALSE(r.value()[0].is_online);
}
