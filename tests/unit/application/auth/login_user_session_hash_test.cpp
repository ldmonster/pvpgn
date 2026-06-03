// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for the second LoginUser overload — execute(LoginWithSessionHashRequest),
// the W3-style session-hash login path. The main login_user_test only covers
// execute(LoginRequest); this drives the session-hash branches: missing hasher,
// unknown user, wrong proof, and the happy path (proof computed via the hasher).

#include <cstdint>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "application/auth/login_user.hpp"
#include "core/clock.hpp"
#include "domain/identity/account.hpp"
#include "domain/identity/ports.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"
#include "infra/inmemory/account_repository.hpp"
#include "infra/inmemory/event_bus.hpp"
#include "infra/inmemory/session_registry.hpp"

namespace {

using namespace pvpgn;
using application::auth::LoginError;
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

// Deterministic fake: a session hash that depends only on (ticks, sessionkey)
// so the test can reproduce the expected proof without real crypto.
class FakeHasher final : public domain::identity::IPasswordHasher {
public:
    domain::BNHash derive_session_hash(const domain::BNHash& /*hash1*/,
                                       std::uint32_t ticks,
                                       std::uint32_t sessionkey) const noexcept override {
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
    domain::ClientTag star = domain::ClientTag::parse("STAR").value();
    domain::IpAddress ip;

    void seed_alice() {
        auto a = domain::identity::Account::create(
            alice_id, make_name("Alice"), make_hash(0xAA), domain::Locale{}).value();
        (void)a.drain_events();
        REQUIRE(accounts.save(a));
    }

    LoginUser with_hasher()    { return LoginUser{accounts, sessions, bus, clock, hasher}; }
    LoginUser without_hasher() { return LoginUser{accounts, sessions, bus, clock}; }

    LoginWithSessionHashRequest req(std::string_view who, domain::BNHash proof,
                                    std::uint32_t ticks, std::uint32_t key,
                                    domain::SessionId session) {
        return LoginWithSessionHashRequest{make_name(who), proof, ticks, key,
                                           star, ip, session};
    }
};

}  // namespace

TEST_CASE("LoginUser(session-hash): missing hasher returns Internal",
          "[application][auth][login_user][session_hash]") {
    Fixture f;
    f.seed_alice();
    auto uc = f.without_hasher();  // 4-arg ctor: hasher_ == nullptr

    auto r = uc.execute(f.req("Alice", make_hash(0), 1u, 2u, domain::SessionId{1}));

    REQUIRE_FALSE(r);
    CHECK(r.error() == LoginError::Internal);
}

TEST_CASE("LoginUser(session-hash): unknown user is rejected",
          "[application][auth][login_user][session_hash]") {
    Fixture f;
    auto uc = f.with_hasher();

    auto r = uc.execute(f.req("ghost", make_hash(0), 1u, 2u, domain::SessionId{1}));

    REQUIRE_FALSE(r);
    CHECK(r.error() == LoginError::UnknownUser);
}

TEST_CASE("LoginUser(session-hash): wrong proof returns InvalidCredentials",
          "[application][auth][login_user][session_hash]") {
    Fixture f;
    f.seed_alice();
    auto uc = f.with_hasher();

    // A proof that does not match the hasher's derived value.
    auto r = uc.execute(f.req("Alice", make_hash(0xFF), 100u, 200u,
                              domain::SessionId{1}));

    REQUIRE_FALSE(r);
    CHECK(r.error() == LoginError::InvalidCredentials);
}

TEST_CASE("LoginUser(session-hash): correct proof authenticates",
          "[application][auth][login_user][session_hash]") {
    Fixture f;
    f.seed_alice();
    auto uc = f.with_hasher();

    const std::uint32_t ticks = 0x1234u;
    const std::uint32_t key   = 0x5678u;
    // Reproduce the server's expected proof: derive_session_hash ignores hash1.
    const domain::BNHash proof =
        f.hasher.derive_session_hash(make_hash(0xAA), ticks, key);

    auto r = uc.execute(f.req("Alice", proof, ticks, key, domain::SessionId{7}));

    REQUIRE(r);
    CHECK(r.value().id.value() == f.alice_id.value());
}
