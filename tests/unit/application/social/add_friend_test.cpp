// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::social::AddFriend`.

#include <initializer_list>
#include <memory>
#include <string_view>
#include <utility>

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

// Helper: a fresh account repo seeded with the named accounts. The repos are
// non-copyable (shared_mutex), so we build a shared_ptr and save through it
// rather than copying a value repo into make_shared.
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

AddFriend make_uc(std::shared_ptr<infra::inmemory::InMemoryAccountRepository> accounts) {
    return AddFriend{
        std::move(accounts),
        std::make_shared<infra::inmemory::InMemoryFriendListRepository>(),
        std::make_shared<infra::inmemory::InMemoryEventBus>()};
}

}  // namespace

TEST_CASE("AddFriend: happy path succeeds when both accounts exist",
          "[application][social][add_friend]") {
    domain::AccountId alice{1};
    domain::AccountId bob{2};
    auto accounts = make_accounts({{alice, "Alice"}, {bob, "Bobby"}});

    auto uc = make_uc(accounts);
    auto r  = uc.execute(alice, bob);

    REQUIRE(r);
}

TEST_CASE("AddFriend: unknown owner returns OwnerNotFound",
          "[application][social][add_friend]") {
    domain::AccountId alice{1};
    domain::AccountId bob{2};
    // Only bob exists — alice is unknown
    auto accounts = make_accounts({{bob, "Bobby"}});

    auto uc = make_uc(accounts);
    auto r  = uc.execute(alice, bob);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == AddFriendError::OwnerNotFound);
}

TEST_CASE("AddFriend: unknown target returns TargetNotFound",
          "[application][social][add_friend]") {
    domain::AccountId alice{1};
    domain::AccountId bob{2};
    // Only alice exists — bob is unknown
    auto accounts = make_accounts({{alice, "Alice"}});

    auto uc = make_uc(accounts);
    auto r  = uc.execute(alice, bob);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == AddFriendError::TargetNotFound);
}

TEST_CASE("AddFriend: adding self returns SelfFriend",
          "[application][social][add_friend]") {
    domain::AccountId alice{1};
    auto accounts = make_accounts({{alice, "Alice"}});

    auto uc = make_uc(accounts);
    auto r  = uc.execute(alice, alice);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == AddFriendError::SelfFriend);
}
