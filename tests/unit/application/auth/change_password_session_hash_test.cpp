// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for the session-hash overload of ChangePasswordUseCase —
// execute(ChangePasswordWithSessionHashRequest). The main change_password_test
// covers only execute(ChangePasswordRequest); this drives the hash2 branches:
// missing hasher, unknown user, wrong proof, no-op rotation, and the happy path.

#include <cstdint>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "application/auth/change_password.hpp"
#include "domain/identity/account.hpp"
#include "domain/identity/ports.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"
#include "infra/inmemory/account_repository.hpp"
#include "infra/inmemory/event_bus.hpp"

namespace {

using namespace pvpgn;
using application::auth::ChangePasswordError;
using application::auth::ChangePasswordUseCase;
using application::auth::ChangePasswordWithSessionHashRequest;

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

class FakeHasher final : public domain::identity::IPasswordHasher {
public:
    domain::BNHash derive_session_hash(const domain::BNHash& /*hash1*/,
                                       std::uint32_t ticks,
                                       std::uint32_t sessionkey) const noexcept override {
        domain::BNHash::Bytes b{};
        b[0] = static_cast<unsigned char>(ticks & 0xFF);
        b[1] = static_cast<unsigned char>(sessionkey & 0xFF);
        b[2] = 0xC3;
        return domain::BNHash{b};
    }
};

struct Fixture {
    infra::inmemory::InMemoryAccountRepository accounts;
    infra::inmemory::InMemoryEventBus          bus;
    FakeHasher                                 hasher;

    domain::AccountId alice_id{42};
    domain::BNHash    current = make_hash(0xAA);

    void seed_alice() {
        auto a = domain::identity::Account::create(
            alice_id, make_name("Alice"), current, domain::Locale{}).value();
        (void)a.drain_events();
        REQUIRE(accounts.save(a));
    }

    ChangePasswordUseCase with_hasher()    { return ChangePasswordUseCase{accounts, bus, hasher}; }
    ChangePasswordUseCase without_hasher() { return ChangePasswordUseCase{accounts, bus}; }

    ChangePasswordWithSessionHashRequest req(std::string_view who,
                                             domain::BNHash proof,
                                             std::uint32_t ticks, std::uint32_t key,
                                             domain::BNHash new_pw) {
        return ChangePasswordWithSessionHashRequest{
            make_name(who), proof, ticks, key, new_pw};
    }
};

}  // namespace

TEST_CASE("ChangePassword(session-hash): missing hasher returns Internal",
          "[application][auth][change_password][session_hash]") {
    Fixture f;
    f.seed_alice();
    auto uc = f.without_hasher();

    auto r = uc.execute(f.req("Alice", make_hash(0), 1u, 2u, make_hash(0xBB)));

    REQUIRE_FALSE(r);
    CHECK(r.error() == ChangePasswordError::Internal);
}

TEST_CASE("ChangePassword(session-hash): unknown user is rejected",
          "[application][auth][change_password][session_hash]") {
    Fixture f;
    auto uc = f.with_hasher();

    auto r = uc.execute(f.req("ghost", make_hash(0), 1u, 2u, make_hash(0xBB)));

    REQUIRE_FALSE(r);
    CHECK(r.error() == ChangePasswordError::UnknownUser);
}

TEST_CASE("ChangePassword(session-hash): wrong proof rejects",
          "[application][auth][change_password][session_hash]") {
    Fixture f;
    f.seed_alice();
    auto uc = f.with_hasher();

    auto r = uc.execute(f.req("Alice", make_hash(0xFF), 9u, 9u, make_hash(0xBB)));

    REQUIRE_FALSE(r);
    CHECK(r.error() == ChangePasswordError::InvalidCurrentPassword);
}

TEST_CASE("ChangePassword(session-hash): no-op rotation is rejected",
          "[application][auth][change_password][session_hash]") {
    Fixture f;
    f.seed_alice();
    auto uc = f.with_hasher();

    const std::uint32_t ticks = 5u, key = 6u;
    auto proof = f.hasher.derive_session_hash(f.current, ticks, key);
    // new password == the current stored password -> PasswordUnchanged.
    auto r = uc.execute(f.req("Alice", proof, ticks, key, f.current));

    REQUIRE_FALSE(r);
    CHECK(r.error() == ChangePasswordError::PasswordUnchanged);
}

TEST_CASE("ChangePassword(session-hash): correct proof rotates the password",
          "[application][auth][change_password][session_hash]") {
    Fixture f;
    f.seed_alice();
    auto uc = f.with_hasher();

    const std::uint32_t ticks = 0x10u, key = 0x20u;
    auto proof = f.hasher.derive_session_hash(f.current, ticks, key);
    auto r = uc.execute(f.req("Alice", proof, ticks, key, make_hash(0xBB)));

    REQUIRE(r);
    CHECK(r.value().value() == f.alice_id.value());
}
