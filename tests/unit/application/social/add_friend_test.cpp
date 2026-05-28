// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::social::AddFriend`.

#include <catch2/catch_test_macros.hpp>

#include "application/social/add_friend.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"
#include "domain/identity/account.hpp"
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

struct Fixture {
    infra::inmemory::InMemoryAccountRepository    accounts;
    infra::inmemory::InMemoryFriendListRepository friend_lists;
    infra::inmemory::InMemoryEventBus             bus;

    domain::AccountId alice_id{1};
    domain::AccountId bob_id{2};

    void seed_alice() {
        auto a = domain::identity::Account::create(
            alice_id, make_name("Alice"), make_hash(0xAA),
            domain::Locale{}).value();
        (void)a.drain_events();
        REQUIRE(accounts.save(a));
    }

    void seed_bob() {
        auto b = domain::identity::Account::create(
            bob_id, make_name("Bobby"), make_hash(0xBB),
            domain::Locale{}).value();
        (void)b.drain_events();
        REQUIRE(accounts.save(b));
    }

    AddFriend make_uc() {
        return AddFriend{
            std::make_shared<infra::inmemory::InMemoryAccountRepository>(accounts),
            std::make_shared<infra::inmemory::InMemoryFriendListRepository>(),
            std::make_shared<infra::inmemory::InMemoryEventBus>()};
    }
};

}  // namespace

TEST_CASE("AddFriend: happy path succeeds when both accounts exist",
          "[application][social][add_friend]") {
    infra::inmemory::InMemoryAccountRepository    accounts;
    infra::inmemory::InMemoryFriendListRepository friend_lists;
    infra::inmemory::InMemoryEventBus             bus;

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

    auto accounts_ptr      = std::make_shared<infra::inmemory::InMemoryAccountRepository>(accounts);
    auto friend_lists_ptr  = std::make_shared<infra::inmemory::InMemoryFriendListRepository>();
    auto bus_ptr           = std::make_shared<infra::inmemory::InMemoryEventBus>();

    AddFriend uc{accounts_ptr, friend_lists_ptr, bus_ptr};
    auto r = uc.execute(alice, bob);

    REQUIRE(r);
}

TEST_CASE("AddFriend: unknown owner returns OwnerNotFound",
          "[application][social][add_friend]") {
    infra::inmemory::InMemoryAccountRepository    accounts;
    infra::inmemory::InMemoryFriendListRepository friend_lists;
    infra::inmemory::InMemoryEventBus             bus;

    domain::AccountId alice{1};
    domain::AccountId bob{2};

    // Only bob exists — alice is unknown
    auto b = domain::identity::Account::create(
        bob, make_name("Bobby"), make_hash(0xBB), domain::Locale{}).value();
    (void)b.drain_events();
    REQUIRE(accounts.save(b));

    AddFriend uc{
        std::make_shared<infra::inmemory::InMemoryAccountRepository>(accounts),
        std::make_shared<infra::inmemory::InMemoryFriendListRepository>(),
        std::make_shared<infra::inmemory::InMemoryEventBus>()};

    auto r = uc.execute(alice, bob);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == AddFriendError::OwnerNotFound);
}

TEST_CASE("AddFriend: unknown target returns TargetNotFound",
          "[application][social][add_friend]") {
    infra::inmemory::InMemoryAccountRepository    accounts;
    infra::inmemory::InMemoryFriendListRepository friend_lists;
    infra::inmemory::InMemoryEventBus             bus;

    domain::AccountId alice{1};
    domain::AccountId bob{2};

    // Only alice exists — bob is unknown
    auto a = domain::identity::Account::create(
        alice, make_name("Alice"), make_hash(0xAA), domain::Locale{}).value();
    (void)a.drain_events();
    REQUIRE(accounts.save(a));

    AddFriend uc{
        std::make_shared<infra::inmemory::InMemoryAccountRepository>(accounts),
        std::make_shared<infra::inmemory::InMemoryFriendListRepository>(),
        std::make_shared<infra::inmemory::InMemoryEventBus>()};

    auto r = uc.execute(alice, bob);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == AddFriendError::TargetNotFound);
}

TEST_CASE("AddFriend: adding self returns SelfFriend",
          "[application][social][add_friend]") {
    infra::inmemory::InMemoryAccountRepository    accounts;
    infra::inmemory::InMemoryFriendListRepository friend_lists;
    infra::inmemory::InMemoryEventBus             bus;

    domain::AccountId alice{1};

    auto a = domain::identity::Account::create(
        alice, make_name("Alice"), make_hash(0xAA), domain::Locale{}).value();
    (void)a.drain_events();
    REQUIRE(accounts.save(a));

    AddFriend uc{
        std::make_shared<infra::inmemory::InMemoryAccountRepository>(accounts),
        std::make_shared<infra::inmemory::InMemoryFriendListRepository>(),
        std::make_shared<infra::inmemory::InMemoryEventBus>()};

    auto r = uc.execute(alice, alice);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == AddFriendError::SelfFriend);
}
