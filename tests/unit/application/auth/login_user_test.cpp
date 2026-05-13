// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::auth::LoginUser`. Exercises the use-case
// against the in-memory port adapters defined in `infra/inmemory/`.

#include <atomic>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "application/auth/login_user.hpp"
#include "core/clock.hpp"
#include "domain/identity/account.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/ip_address.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"
#include "infra/inmemory/account_repository.hpp"
#include "infra/inmemory/event_bus.hpp"
#include "infra/inmemory/session_registry.hpp"

namespace {

using namespace pvpgn;
using application::auth::LoginError;
using application::auth::LoginRequest;
using application::auth::LoginUser;

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
    infra::inmemory::InMemoryEventBus          bus;
    core::ManualClock                          clock{core::SystemTime{}};

    domain::AccountId  alice_id{42};
    domain::SessionId  alice_session{1};
    domain::ClientTag  star_tag = domain::ClientTag::parse("STAR").value();
    domain::IpAddress  ip;
    domain::BNHash     password = make_hash(0xAA);

    void seed_alice() {
        auto a = domain::identity::Account::create(
            alice_id, make_name("Alice"), password, domain::Locale{}).value();
        (void)a.drain_events();
        REQUIRE(accounts.save(a));
    }

    LoginUser make_use_case() {
        return LoginUser{accounts, sessions, bus, clock};
    }

    LoginRequest req_for(std::string_view who,
                         const domain::BNHash& candidate,
                         domain::SessionId session) {
        return LoginRequest{make_name(who), candidate, star_tag, ip, session};
    }
};

}  // namespace

TEST_CASE("LoginUser: accepts a known user with the right password",
          "[application][auth][login]") {
    Fixture f;
    f.seed_alice();

    std::atomic<int> events{0};
    f.bus.subscribe([&](const domain::events::DomainEvent&) { ++events; });

    auto uc = f.make_use_case();
    auto r = uc.execute(f.req_for("alice", f.password, f.alice_session));

    REQUIRE(r);
    REQUIRE(r.value().id == f.alice_id);
    REQUIRE(f.sessions.account_for(f.alice_session).has_value());
    REQUIRE(events.load() >= 1);  // UserLoggedIn at least
}

TEST_CASE("LoginUser: unknown user is rejected",
          "[application][auth][login]") {
    Fixture f;
    auto uc = f.make_use_case();
    auto r = uc.execute(f.req_for("ghost", f.password, f.alice_session));

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == LoginError::UnknownUser);
}

TEST_CASE("LoginUser: bad password is rejected and emits an event",
          "[application][auth][login]") {
    Fixture f;
    f.seed_alice();

    std::atomic<int> events{0};
    f.bus.subscribe([&](const domain::events::DomainEvent&) { ++events; });

    auto uc = f.make_use_case();
    auto r = uc.execute(f.req_for("alice", make_hash(0xBB), f.alice_session));

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == LoginError::InvalidCredentials);
    REQUIRE_FALSE(f.sessions.account_for(f.alice_session).has_value());
    REQUIRE(events.load() == 1);  // UserLoginRejected
}

TEST_CASE("LoginUser: duplicate session is rejected",
          "[application][auth][login]") {
    Fixture f;
    f.seed_alice();

    auto uc = f.make_use_case();
    REQUIRE(uc.execute(f.req_for("alice", f.password, f.alice_session)));

    // Same account, different session — single-session policy refuses.
    auto r = uc.execute(f.req_for("alice", f.password, domain::SessionId{2}));
    REQUIRE_FALSE(r);
    REQUIRE(r.error() == LoginError::AlreadyLoggedIn);
}

TEST_CASE("LoginUser: locked account is rejected",
          "[application][auth][login]") {
    Fixture f;
    auto a = domain::identity::Account::create(
        f.alice_id, make_name("Alice"), f.password, domain::Locale{}).value();
    (void)a.drain_events();
    a.lock();
    (void)a.drain_events();
    REQUIRE(f.accounts.save(a));

    auto uc = f.make_use_case();
    auto r = uc.execute(f.req_for("alice", f.password, f.alice_session));

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == LoginError::Locked);
}
