// SPDX-License-Identifier: GPL-2.0-or-later
//
// Net-new coverage for `application::auth::ChangePasswordUseCase`'s
// persistence-failure branch. The wrong-old-password / unknown-user / no-op /
// happy / rotation-clear paths are already covered by change_password_test.cpp
// and change_password_session_hash_test.cpp. This file drives the otherwise-
// uncovered `ChangePasswordError::PersistenceFailed` arm (account.save() fails
// after the old-password check passes) for BOTH execute() overloads, and
// asserts that NO events are published when the durable write fails.

#include <atomic>
#include <cstdint>
#include <functional>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "application/auth/change_password.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/identity/account.hpp"
#include "domain/identity/ports.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/events.hpp"
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
using application::auth::ChangePasswordWithSessionHashRequest;

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

// Reads served from a real in-memory store; save() always fails.
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

    void seed(const domain::identity::Account& a) { REQUIRE(inner_.save(a)); }

private:
    infra::inmemory::InMemoryAccountRepository inner_;
};

// Deterministic session-hash fake.
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
    FailingSaveRepository             accounts;
    infra::inmemory::InMemoryEventBus bus;
    FakeHasher                        hasher;

    domain::AccountId id{99};
    domain::BNHash    current = hash_fill(0x11);
    domain::BNHash    next    = hash_fill(0x22);

    void seed() {
        auto a = domain::identity::Account::create(
            id, mk_name("Bob"), current, domain::Locale{}).value();
        (void)a.drain_events();
        accounts.seed(a);
    }
};

}  // namespace

TEST_CASE("ChangePasswordUseCase: save failure surfaces PersistenceFailed "
          "and publishes no events",
          "[application][auth][change_password][persistence]") {
    Fixture f;
    f.seed();

    std::atomic<int> count{0};
    f.bus.subscribe([&](const domain::events::DomainEvent&) { ++count; });

    ChangePasswordUseCase uc{f.accounts, f.bus};
    auto r = uc.execute(ChangePasswordRequest{mk_name("bob"), f.current, f.next});

    REQUIRE_FALSE(r);
    CHECK(r.error() == ChangePasswordError::PersistenceFailed);
    CHECK(count.load() == 0);  // events dropped when the write is not durable
}

TEST_CASE("ChangePasswordUseCase(session-hash): save failure surfaces "
          "PersistenceFailed and publishes no events",
          "[application][auth][change_password][session_hash][persistence]") {
    Fixture f;
    f.seed();

    std::atomic<int> count{0};
    f.bus.subscribe([&](const domain::events::DomainEvent&) { ++count; });

    const std::uint32_t ticks = 0x1234u;
    const std::uint32_t key   = 0x5678u;
    const domain::BNHash proof =
        f.hasher.derive_session_hash(f.current, ticks, key);

    ChangePasswordUseCase uc{f.accounts, f.bus, f.hasher};
    auto r = uc.execute(ChangePasswordWithSessionHashRequest{
        mk_name("bob"), proof, ticks, key, f.next});

    REQUIRE_FALSE(r);
    CHECK(r.error() == ChangePasswordError::PersistenceFailed);
    CHECK(count.load() == 0);
}
