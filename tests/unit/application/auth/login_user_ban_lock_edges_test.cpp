// SPDX-License-Identifier: GPL-2.0-or-later
//
// Coverage for `application::auth::LoginUser`. The base
// login_user_test covers unknown/bad-password/locked/duplicate-session/
// must-change/PersistenceFailed for the password overload, and missing-
// hasher/unknown/wrong-proof/happy for the session-hash overload.
//
// This file fills the remaining uncovered arms:
//   * password overload: the Banned outcome (active ban -> LoginError::Banned)
//   * session-hash overload: Banned, Locked, MustChangePassword, AlreadyLoggedIn
//     (the post-proof aggregate gate + single-session policy paths)
//   * session-hash overload: the wrong-proof rejection-event publication.

#include <atomic>
#include <cstdint>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "application/auth/login_user.hpp"
#include "core/clock.hpp"
#include "domain/identity/account.hpp"
#include "domain/identity/ports.hpp"
#include "domain/shared/ban.hpp"
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
using application::auth::LoginWithSessionHashRequest;

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

// Deterministic session-hash fake (mirrors login_user_session_hash_test.cpp).
class FakeHasher final : public domain::identity::IPasswordHasher {
public:
    domain::BNHash derive_session_hash(const domain::BNHash& /*hash1*/,
                                       std::uint32_t ticks,
                                       std::uint32_t sessionkey)
        const noexcept override {
        domain::BNHash::Bytes b{};
        b[0] = static_cast<unsigned char>(ticks & 0xFF);
        b[1] = static_cast<unsigned char>((ticks >> 8) & 0xFF);
        b[2] = static_cast<unsigned char>(sessionkey & 0xFF);
        b[3] = 0xAB;
        return domain::BNHash{b};
    }
};

struct Fixture {
    infra::inmemory::InMemoryAccountRepository accounts;
    infra::inmemory::InMemorySessionRegistry   sessions;
    infra::inmemory::InMemoryEventBus          bus;
    core::ManualClock                          clock{core::SystemTime{}};
    FakeHasher                                 hasher;

    domain::AccountId alice_id{42};
    domain::SessionId alice_session{1};
    domain::ClientTag star = domain::ClientTag::parse("STAR").value();
    domain::IpAddress ip;
    domain::BNHash    password = make_hash(0xAA);

    // Persist an account, optionally mutated by a caller-supplied hook,
    // draining any factory/command events so the bus starts clean.
    template <class Mutate>
    void seed_with(Mutate&& m) {
        auto a = domain::identity::Account::create(
            alice_id, make_name("Alice"), password, domain::Locale{}).value();
        std::forward<Mutate>(m)(a);
        (void)a.drain_events();
        REQUIRE(accounts.save(a));
    }

    LoginUser with_hasher()    { return LoginUser{accounts, sessions, bus, clock, hasher}; }
    LoginUser without_hasher() { return LoginUser{accounts, sessions, bus, clock}; }

    LoginRequest pw_req(const domain::BNHash& candidate) {
        return LoginRequest{make_name("Alice"), candidate, star, ip, alice_session};
    }

    // Build a session-hash request whose proof matches the hasher for the
    // given (ticks,key) — i.e. credentials are correct.
    LoginWithSessionHashRequest sh_good_req(std::uint32_t ticks,
                                            std::uint32_t key,
                                            domain::SessionId session) {
        const domain::BNHash proof =
            hasher.derive_session_hash(password, ticks, key);
        return LoginWithSessionHashRequest{
            make_name("Alice"), proof, ticks, key, star, ip, session};
    }
};

// An active (never-expiring) ban. We pick a far-future expiry so
// `active_at(now)` is true for the fixture's epoch clock.
domain::Ban make_active_ban() {
    domain::Ban b{};
    b.scope      = domain::BanScope::Account;
    b.reason     = "test ban";
    b.issuer     = domain::AccountId{1};
    b.issued_at  = core::SystemTime{};
    b.expires_at = std::nullopt;  // permanent => always active
    return b;
}

}  // namespace

TEST_CASE("LoginUser(password): banned account is rejected with Banned",
          "[application][auth][login][ban]") {
    Fixture f;
    f.seed_with([&](domain::identity::Account& a) {
        a.apply_ban(make_active_ban());
    });

    auto uc = f.with_hasher();  // hasher irrelevant for the password overload
    auto r  = uc.execute(f.pw_req(f.password));

    REQUIRE_FALSE(r);
    CHECK(r.error() == LoginError::Banned);
    CHECK_FALSE(f.sessions.account_for(f.alice_session).has_value());
}

TEST_CASE("LoginUser(session-hash): banned account is rejected with Banned",
          "[application][auth][login][session_hash][ban]") {
    Fixture f;
    f.seed_with([&](domain::identity::Account& a) {
        a.apply_ban(make_active_ban());
    });

    std::atomic<int> events{0};
    f.bus.subscribe([&](const domain::events::DomainEvent&) { ++events; });

    auto uc = f.with_hasher();
    auto r  = uc.execute(f.sh_good_req(0x11u, 0x22u, f.alice_session));

    REQUIRE_FALSE(r);
    CHECK(r.error() == LoginError::Banned);
    // The aggregate emits a UserLoginRejected(AccountBanned) which the
    // use-case drains and publishes even on the failure path.
    CHECK(events.load() >= 1);
}

TEST_CASE("LoginUser(session-hash): locked account is rejected with Locked",
          "[application][auth][login][session_hash][lock]") {
    Fixture f;
    f.seed_with([&](domain::identity::Account& a) { a.lock(); });

    auto uc = f.with_hasher();
    auto r  = uc.execute(f.sh_good_req(0x33u, 0x44u, f.alice_session));

    REQUIRE_FALSE(r);
    CHECK(r.error() == LoginError::Locked);
}

TEST_CASE("LoginUser(session-hash): must-change-password surfaces "
          "MustChangePassword",
          "[application][auth][login][session_hash][must_change_password]") {
    Fixture f;
    f.seed_with([&](domain::identity::Account& a) {
        a.require_password_change();
    });

    auto uc = f.with_hasher();
    auto r  = uc.execute(f.sh_good_req(0x55u, 0x66u, f.alice_session));

    REQUIRE_FALSE(r);
    CHECK(r.error() == LoginError::MustChangePassword);
    CHECK_FALSE(f.sessions.account_for(f.alice_session).has_value());
}

TEST_CASE("LoginUser(session-hash): a second login kicks the previous session",
          "[application][auth][login][session_hash]") {
    Fixture f;
    f.seed_with([](domain::identity::Account&) {});

    auto uc = f.with_hasher();
    // First login attaches alice_session.
    REQUIRE(uc.execute(f.sh_good_req(0x77u, 0x88u, f.alice_session)));

    // Second login for the same account on a different session succeeds via
    // kick-old-login and reports the displaced (old) session.
    auto r = uc.execute(f.sh_good_req(0x77u, 0x88u, domain::SessionId{2}));
    REQUIRE(r);
    REQUIRE(r.value().kicked_session.has_value());
    CHECK(r.value().kicked_session.value() == f.alice_session);
}

TEST_CASE("LoginUser(session-hash): wrong proof publishes a rejection event",
          "[application][auth][login][session_hash]") {
    Fixture f;
    f.seed_with([](domain::identity::Account&) {});

    std::atomic<int> events{0};
    f.bus.subscribe([&](const domain::events::DomainEvent&) { ++events; });

    auto uc = f.with_hasher();
    // Proof does not match the hasher's derived value for (1,2).
    auto r  = uc.execute(LoginWithSessionHashRequest{
        make_name("Alice"), make_hash(0xFF), 1u, 2u, f.star, f.ip,
        f.alice_session});

    REQUIRE_FALSE(r);
    CHECK(r.error() == LoginError::InvalidCredentials);
    // The use-case publishes a UserLoginRejected(InvalidCredentials) for
    // audit parity before the aggregate is even consulted.
    CHECK(events.load() == 1);
    CHECK_FALSE(f.sessions.account_for(f.alice_session).has_value());
}
