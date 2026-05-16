// SPDX-License-Identifier: GPL-2.0-or-later
//
// Batch 27d: `ChangePasswordUseCase` -- pure-domain rotation flow.

#include <atomic>
#include <string_view>
#include <variant>

#include <catch2/catch_test_macros.hpp>

#include "application/auth/change_password.hpp"
#include "domain/shared/events.hpp"
#include "domain/identity/account.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"
#include "infra/inmemory/account_repository.hpp"
#include "infra/inmemory/event_bus.hpp"

namespace {

using namespace pvpgn;
using application::auth::ChangePasswordError;
using application::auth::ChangePasswordRequest;
using application::auth::ChangePasswordUseCase;

domain::BNHash hash_fill(std::uint8_t fill) {
    domain::BNHash::Bytes b{};
    b.fill(fill);
    return domain::BNHash{b};
}

domain::UserName mk_name(std::string_view s) {
    auto r = domain::UserName::parse(s);
    REQUIRE(r);
    return r.value();
}

struct Fixture {
    infra::inmemory::InMemoryAccountRepository accounts;
    infra::inmemory::InMemoryEventBus          bus;
    domain::AccountId id{99};
    domain::BNHash    current = hash_fill(0x11);
    domain::BNHash    next    = hash_fill(0x22);

    void seed(bool must_change = false) {
        domain::identity::Account a = must_change
            ? domain::identity::Account::rehydrate(
                  id, mk_name("Bob"), current, domain::Locale{},
                  /*groups=*/{},
                  /*ban=*/std::nullopt,
                  /*locked=*/false,
                  /*must_change_password=*/true)
            : domain::identity::Account::create(
                  id, mk_name("Bob"), current, domain::Locale{}).value();
        (void)a.drain_events();
        REQUIRE(accounts.save(a));
    }

    ChangePasswordUseCase uc() { return ChangePasswordUseCase{accounts, bus}; }

    ChangePasswordRequest req(const domain::BNHash& cur,
                              const domain::BNHash& nxt) {
        return ChangePasswordRequest{mk_name("bob"), cur, nxt};
    }
};

}  // namespace

TEST_CASE("ChangePasswordUseCase: happy path rotates and publishes",
          "[application][auth][change_password]") {
    Fixture f;
    f.seed();
    std::atomic<int> count{0};
    bool saw_changed = false;
    f.bus.subscribe([&](const domain::events::DomainEvent& e) {
        ++count;
        if (std::holds_alternative<domain::events::AccountPasswordChanged>(e)) {
            saw_changed = true;
        }
    });

    auto r = f.uc().execute(f.req(f.current, f.next));
    REQUIRE(r);
    REQUIRE(r.value() == f.id);
    REQUIRE(saw_changed);
    REQUIRE(count.load() == 1);  // no rotation flag => no Cleared event

    // Repository now holds the new hash.
    auto reloaded = f.accounts.find_by_name(mk_name("bob").canonical());
    REQUIRE(reloaded);
    REQUIRE(reloaded.value().verify_password(f.next));
}

TEST_CASE("ChangePasswordUseCase: wrong current password rejects",
          "[application][auth][change_password]") {
    Fixture f;
    f.seed();
    std::atomic<int> count{0};
    f.bus.subscribe([&](const domain::events::DomainEvent&) { ++count; });

    auto r = f.uc().execute(f.req(hash_fill(0x99), f.next));
    REQUIRE_FALSE(r);
    REQUIRE(r.error() == ChangePasswordError::InvalidCurrentPassword);
    REQUIRE(count.load() == 0);
}

TEST_CASE("ChangePasswordUseCase: rejects no-op rotation",
          "[application][auth][change_password]") {
    Fixture f;
    f.seed();
    std::atomic<int> count{0};
    f.bus.subscribe([&](const domain::events::DomainEvent&) { ++count; });

    auto r = f.uc().execute(f.req(f.current, f.current));
    REQUIRE_FALSE(r);
    REQUIRE(r.error() == ChangePasswordError::PasswordUnchanged);
    REQUIRE(count.load() == 0);
}

TEST_CASE("ChangePasswordUseCase: unknown user surfaces UnknownUser",
          "[application][auth][change_password]") {
    Fixture f;
    auto r = f.uc().execute(f.req(f.current, f.next));
    REQUIRE_FALSE(r);
    REQUIRE(r.error() == ChangePasswordError::UnknownUser);
}

TEST_CASE("ChangePasswordUseCase: clears must_change_password flag and "
          "emits AccountPasswordRotationCleared",
          "[application][auth][change_password]") {
    Fixture f;
    f.seed(/*must_change=*/true);
    bool saw_cleared = false;
    bool saw_changed = false;
    f.bus.subscribe([&](const domain::events::DomainEvent& e) {
        if (std::holds_alternative<
                domain::events::AccountPasswordRotationCleared>(e)) {
            saw_cleared = true;
        }
        if (std::holds_alternative<domain::events::AccountPasswordChanged>(e)) {
            saw_changed = true;
        }
    });

    auto r = f.uc().execute(f.req(f.current, f.next));
    REQUIRE(r);
    REQUIRE(saw_changed);
    REQUIRE(saw_cleared);

    auto reloaded = f.accounts.find_by_name(mk_name("bob").canonical());
    REQUIRE(reloaded);
    REQUIRE_FALSE(reloaded.value().must_change_password());
}
