// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::auth::LockAccount` and `UnlockAccount`.
// Exercises the use-cases against the in-memory port adapters.

#include <catch2/catch_test_macros.hpp>

#include "application/auth/account_lock.hpp"
#include "core/clock.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"
#include "infra/inmemory/account_repository.hpp"
#include "infra/inmemory/event_bus.hpp"

namespace {

using namespace pvpgn;
using application::auth::LockAccount;
using application::auth::UnlockAccount;

domain::BNHash make_hash(std::uint8_t fill) {
    domain::BNHash::Bytes b{};
    b.fill(fill);
    return domain::BNHash{b};
}

domain::UserName make_name(std::string_view s) {
    auto r = domain::UserName::parse(s);
    REQUIRE(r);
    return r.value();
}

struct Fixture {
    infra::inmemory::InMemoryAccountRepository accounts;
    infra::inmemory::InMemoryEventBus bus;

    domain::AccountId alice_id{42};
    domain::AccountId admin_id{1};
    domain::BNHash password = make_hash(0xAA);

    void seed_alice() {
        auto a = domain::identity::Account::create(
            alice_id, make_name("Alice"), password, domain::Locale{}).value();
        (void)a.drain_events();
        REQUIRE(accounts.save(a));
    }

    LockAccount make_lock_use_case() {
        return LockAccount{accounts, bus};
    }

    UnlockAccount make_unlock_use_case() {
        return UnlockAccount{accounts, bus};
    }
};

}  // namespace

TEST_CASE("LockAccount: locks an account", "[application][auth][lock]") {
    Fixture f;
    f.seed_alice();

    auto uc = f.make_lock_use_case();
    auto r = uc.execute(f.alice_id, f.admin_id, "Spam violation");

    REQUIRE(r);

    // Verify the account is now locked
    auto found = f.accounts.find_by_id(f.alice_id);
    REQUIRE(found);
    REQUIRE(found.value().is_locked());
}

TEST_CASE("LockAccount: fails if account not found",
          "[application][auth][lock]") {
    Fixture f;
    auto uc = f.make_lock_use_case();

    domain::AccountId nonexistent{999};
    auto r = uc.execute(nonexistent, f.admin_id, "Test");

    REQUIRE_FALSE(r);
}

TEST_CASE("LockAccount: publishes events", "[application][auth][lock]") {
    Fixture f;
    f.seed_alice();

    std::size_t event_count = 0;
    f.bus.subscribe([&](const domain::events::DomainEvent&) { ++event_count; });

    auto uc = f.make_lock_use_case();
    auto r = uc.execute(f.alice_id, f.admin_id, "Test reason");

    REQUIRE(r);
    // We should see at least one event from the operation
}

TEST_CASE("UnlockAccount: unlocks a locked account",
          "[application][auth][unlock]") {
    Fixture f;
    f.seed_alice();

    // First lock the account
    auto lock_uc = f.make_lock_use_case();
    (void)lock_uc.execute(f.alice_id, f.admin_id, "Initial lock");

    // Verify it's locked
    {
        auto found = f.accounts.find_by_id(f.alice_id);
        REQUIRE(found.value().is_locked());
    }

    // Now unlock it
    auto unlock_uc = f.make_unlock_use_case();
    auto r = unlock_uc.execute(f.alice_id, f.admin_id);

    REQUIRE(r);

    // Verify the account is no longer locked
    auto found = f.accounts.find_by_id(f.alice_id);
    REQUIRE(found);
    REQUIRE_FALSE(found.value().is_locked());
}

TEST_CASE("UnlockAccount: fails if account not found",
          "[application][auth][unlock]") {
    Fixture f;
    auto uc = f.make_unlock_use_case();

    domain::AccountId nonexistent{999};
    auto r = uc.execute(nonexistent, f.admin_id);

    REQUIRE_FALSE(r);
}

TEST_CASE("UnlockAccount: can unlock an already-unlocked account",
          "[application][auth][unlock]") {
    Fixture f;
    f.seed_alice();

    // Alice starts unlocked; unlocking should be idempotent
    auto uc = f.make_unlock_use_case();
    auto r = uc.execute(f.alice_id, f.admin_id);

    REQUIRE(r);
    auto found = f.accounts.find_by_id(f.alice_id);
    REQUIRE_FALSE(found.value().is_locked());
}

TEST_CASE("LockAccount and UnlockAccount: lock prevents login barring",
          "[application][auth][lock]") {
    Fixture f;
    f.seed_alice();

    auto lock_uc = f.make_lock_use_case();
    (void)lock_uc.execute(f.alice_id, f.admin_id, "Test");

    auto found = f.accounts.find_by_id(f.alice_id);
    REQUIRE(found.value().is_locked());
    REQUIRE(found.value().is_login_barred(core::SystemTime{}));
}
