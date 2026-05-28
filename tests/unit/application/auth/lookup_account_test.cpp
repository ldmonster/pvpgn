// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::auth::LookupAccountByName`. Exercises the
// use-case against the in-memory port adapters defined in
// `infra/inmemory/`.

#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "application/auth/lookup_account.hpp"
#include "domain/identity/account.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"
#include "infra/inmemory/account_repository.hpp"
#include "infra/inmemory/session_registry.hpp"

namespace {

using namespace pvpgn;
using application::auth::LookupAccountByName;
using application::auth::LookupAccountResult;

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
    infra::inmemory::InMemorySessionRegistry   sessions;

    domain::AccountId alice_id{42};
    domain::SessionId alice_session{1};

    void seed_alice() {
        auto a = domain::identity::Account::create(
            alice_id, make_name("Alice"), make_hash(0xAA),
            domain::Locale{}).value();
        (void)a.drain_events();
        REQUIRE(accounts.save(a));
    }

    LookupAccountByName make_use_case() {
        return LookupAccountByName{accounts, sessions};
    }
};

}  // namespace

TEST_CASE("LookupAccountByName: account found, not online",
          "[application][auth][lookup_account]") {
    Fixture f;
    f.seed_alice();

    auto uc = f.make_use_case();
    auto r  = uc.execute("alice");

    REQUIRE(r);
    REQUIRE(r.value().id        == f.alice_id);
    REQUIRE(r.value().name      == "alice");
    REQUIRE(r.value().is_locked == false);
    REQUIRE(r.value().is_online == false);
}

TEST_CASE("LookupAccountByName: account found, is online",
          "[application][auth][lookup_account]") {
    Fixture f;
    f.seed_alice();
    REQUIRE(f.sessions.attach(f.alice_session, f.alice_id));

    auto uc = f.make_use_case();
    auto r  = uc.execute("alice");

    REQUIRE(r);
    REQUIRE(r.value().id        == f.alice_id);
    REQUIRE(r.value().is_online == true);
}

TEST_CASE("LookupAccountByName: unknown name returns NotFound",
          "[application][auth][lookup_account]") {
    Fixture f;
    // No accounts seeded.

    auto uc = f.make_use_case();
    auto r  = uc.execute("ghost");

    REQUIRE_FALSE(r);
    REQUIRE(r.error().code() == pvpgn::core::StatusCode::NotFound);
}
