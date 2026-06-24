// SPDX-License-Identifier: GPL-2.0-or-later
//
// Net-new coverage for `application::auth::LoginUser`'s persistence-failure
// branch. The happy/locked/unknown/duplicate-session/must-change paths are
// already exercised by login_user_test.cpp and login_user_session_hash_test.cpp;
// this file drives the otherwise-uncovered `LoginError::PersistenceFailed`
// arm (account.save() fails *after* credentials and session-attach succeed),
// for BOTH execute() overloads. It also asserts the session is rolled back
// (detached) when the post-login save fails.

#include <cstdint>
#include <functional>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "application/auth/login_user.hpp"
#include "core/clock.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/identity/account.hpp"
#include "domain/identity/ports.hpp"
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

// A repository whose reads are served from a real in-memory store but whose
// save() always fails — the only way to reach LoginUser's PersistenceFailed
// arm without a real SQL backend.
class FailingSaveRepository final
    : public domain::identity::IAccountRepository {
public:
    core::Result<domain::identity::Account>
    find_by_id(domain::AccountId id) const override {
        return inner_.find_by_id(id);
    }
    core::Result<domain::identity::Account>
    find_by_name(const domain::UserName& name) const override {
        return inner_.find_by_name(name);
    }
    void forEach(
        std::function<bool(const domain::identity::Account&)> p) const override {
        inner_.forEach(std::move(p));
    }
    std::size_t size() const noexcept override { return inner_.size(); }

    core::Status<> save(const domain::identity::Account&) override {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "save deliberately fails"});
    }
    core::Status<> remove(domain::AccountId id) override {
        return inner_.remove(id);
    }

    // Seed bypasses the failing save() so reads can find the account.
    void seed(const domain::identity::Account& a) { REQUIRE(inner_.save(a)); }

private:
    infra::inmemory::InMemoryAccountRepository inner_;
};

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
    FailingSaveRepository                    accounts;
    infra::inmemory::InMemorySessionRegistry sessions;
    infra::inmemory::InMemoryEventBus        bus;
    core::ManualClock                        clock{core::SystemTime{}};
    FakeHasher                               hasher;

    domain::AccountId alice_id{42};
    domain::SessionId alice_session{1};
    domain::ClientTag star = domain::ClientTag::parse("STAR").value();
    domain::IpAddress ip;
    domain::BNHash    password = make_hash(0xAA);

    void seed_alice() {
        auto a = domain::identity::Account::create(
            alice_id, make_name("Alice"), password, domain::Locale{}).value();
        (void)a.drain_events();
        accounts.seed(a);
    }

    LoginRequest req(const domain::BNHash& candidate) {
        return LoginRequest{make_name("Alice"), candidate, star, ip,
                            alice_session};
    }
};

}  // namespace

TEST_CASE("LoginUser: post-login save failure surfaces PersistenceFailed "
          "and rolls back the session",
          "[application][auth][login][persistence]") {
    Fixture f;
    f.seed_alice();

    LoginUser uc{f.accounts, f.sessions, f.bus, f.clock};
    auto r = uc.execute(f.req(f.password));

    REQUIRE_FALSE(r);
    CHECK(r.error() == LoginError::PersistenceFailed);
    // The single-session attach must have been undone on save failure.
    CHECK_FALSE(f.sessions.account_for(f.alice_session).has_value());
}

TEST_CASE("LoginUser(session-hash): post-login save failure surfaces "
          "PersistenceFailed and rolls back the session",
          "[application][auth][login][session_hash][persistence]") {
    Fixture f;
    f.seed_alice();

    const std::uint32_t ticks = 0x1234u;
    const std::uint32_t key   = 0x5678u;
    const domain::BNHash proof =
        f.hasher.derive_session_hash(f.password, ticks, key);

    LoginUser uc{f.accounts, f.sessions, f.bus, f.clock, f.hasher};
    auto r = uc.execute(LoginWithSessionHashRequest{
        make_name("Alice"), proof, ticks, key, f.star, f.ip, f.alice_session});

    REQUIRE_FALSE(r);
    CHECK(r.error() == LoginError::PersistenceFailed);
    CHECK_FALSE(f.sessions.account_for(f.alice_session).has_value());
}
