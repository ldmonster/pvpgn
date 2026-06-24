// SPDX-License-Identifier: GPL-2.0-or-later
//
// Additional outcome tests for `application::social::ListFriends`:
//   * the OwnerNotFound arm (unreachable via the in-memory repo, which returns
//     an empty list for unknown owners, so we use a failing fake);
//   * the "skip friend whose account is missing" branch (the `continue`);
//   * the online path (a friend with an attached session reports is_online).
// Mirrors list_friends_test.cpp for fakes/helpers.

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "application/social/list_friends.hpp"
#include "domain/identity/ports.hpp"
#include "domain/social/ports.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"
#include "domain/identity/account.hpp"
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

// IFriendListRepository whose find_by_owner always fails — the only way to
// drive ListFriends into its OwnerNotFound arm, since the in-memory repo
// substitutes an empty list for unknown owners.
class FailingFriendListRepository final
    : public domain::social::IFriendListRepository {
public:
    core::Result<domain::social::FriendList>
    find_by_owner(domain::AccountId /*owner_id*/) const override {
        return core::fail(
            core::Error{core::StatusCode::NotFound, "owner not found (test)"});
    }

    core::Status<> save(const domain::social::FriendList& /*list*/) override {
        return core::ok();
    }
};

}  // namespace

TEST_CASE("ListFriends: failing owner lookup returns OwnerNotFound",
          "[application][social][list_friends]") {
    auto accounts =
        std::make_shared<infra::inmemory::InMemoryAccountRepository>();

    ListFriends uc{
        std::make_shared<FailingFriendListRepository>(),
        std::make_shared<infra::inmemory::InMemorySessionRegistry>(),
        accounts};

    auto r = uc.execute(domain::AccountId{1});

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == ListFriendsError::OwnerNotFound);
}

TEST_CASE("ListFriends: skips friends whose account no longer exists",
          "[application][social][list_friends]") {
    auto accounts =
        std::make_shared<infra::inmemory::InMemoryAccountRepository>();
    auto friend_lists =
        std::make_shared<infra::inmemory::InMemoryFriendListRepository>();

    domain::AccountId alice{1};
    domain::AccountId bob{2};
    domain::AccountId ghost{3};  // intentionally never saved as an account

    auto a = domain::identity::Account::create(
        alice, make_name("Alice"), make_hash(0xAA), domain::Locale{}).value();
    (void)a.drain_events();
    REQUIRE(accounts->save(a));

    auto b = domain::identity::Account::create(
        bob, make_name("Bobby"), make_hash(0xBB), domain::Locale{}).value();
    (void)b.drain_events();
    REQUIRE(accounts->save(b));

    // Alice befriends bob (resolvable) and ghost (no account -> skipped).
    domain::social::FriendList list{alice};
    list.add(bob);
    list.add(ghost);
    (void)list.drain_events();
    REQUIRE(friend_lists->save(list));

    ListFriends uc{
        friend_lists,
        std::make_shared<infra::inmemory::InMemorySessionRegistry>(),
        accounts};

    auto r = uc.execute(alice);

    REQUIRE(r);
    // Ghost is dropped; only bob survives.
    REQUIRE(r.value().size() == 1);
    REQUIRE(r.value()[0].id.value() == bob.value());
}

TEST_CASE("ListFriends: a friend with an attached session is reported online",
          "[application][social][list_friends]") {
    auto accounts =
        std::make_shared<infra::inmemory::InMemoryAccountRepository>();
    auto friend_lists =
        std::make_shared<infra::inmemory::InMemoryFriendListRepository>();
    auto registry =
        std::make_shared<infra::inmemory::InMemorySessionRegistry>();

    domain::AccountId alice{1};
    domain::AccountId bob{2};

    auto a = domain::identity::Account::create(
        alice, make_name("Alice"), make_hash(0xAA), domain::Locale{}).value();
    (void)a.drain_events();
    REQUIRE(accounts->save(a));

    auto b = domain::identity::Account::create(
        bob, make_name("Bobby"), make_hash(0xBB), domain::Locale{}).value();
    (void)b.drain_events();
    REQUIRE(accounts->save(b));

    domain::social::FriendList list{alice};
    list.add(bob);
    (void)list.drain_events();
    REQUIRE(friend_lists->save(list));

    // Bob is logged in: attach a session so session_for(bob) resolves.
    REQUIRE(registry->attach(domain::SessionId{42}, bob));

    ListFriends uc{friend_lists, registry, accounts};

    auto r = uc.execute(alice);

    REQUIRE(r);
    REQUIRE(r.value().size() == 1);
    REQUIRE(r.value()[0].id.value() == bob.value());
    REQUIRE(r.value()[0].is_online);
    REQUIRE_FALSE(r.value()[0].current_channel.has_value());
    REQUIRE_FALSE(r.value()[0].current_game.has_value());
}
