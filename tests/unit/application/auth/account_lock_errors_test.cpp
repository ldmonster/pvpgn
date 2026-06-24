// SPDX-License-Identifier: GPL-2.0-or-later
//
// Net-new coverage for `application::auth::LockAccount` / `UnlockAccount`.
// account_lock_test.cpp covers happy lock/unlock, not-found, idempotent
// unlock, and event publication. This file drives the otherwise-uncovered
// persistence-failure arm of BOTH use-cases: find() succeeds but save()
// fails, so the use-case returns Internal and never publishes events.

#include <functional>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "application/auth/account_lock.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
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

struct Fixture {
    FailingSaveRepository             accounts;
    infra::inmemory::InMemoryEventBus bus;
    domain::AccountId                 alice_id{42};
    domain::AccountId                 admin_id{1};

    // Seed an account, optionally already locked, draining factory events.
    void seed_alice(bool locked) {
        auto a = domain::identity::Account::create(
            alice_id, make_name("Alice"), make_hash(0xAA),
            domain::Locale{}).value();
        if (locked) a.lock();
        (void)a.drain_events();
        accounts.seed(a);
    }
};

}  // namespace

TEST_CASE("LockAccount: save failure surfaces Internal and emits no events",
          "[application][auth][lock][errors]") {
    Fixture f;
    f.seed_alice(/*locked=*/false);

    std::size_t events = 0;
    f.bus.subscribe([&](const domain::events::DomainEvent&) { ++events; });

    LockAccount uc{f.accounts, f.bus};
    auto r = uc.execute(f.alice_id, f.admin_id, "spam");

    REQUIRE_FALSE(r);
    CHECK(r.error().code() == core::StatusCode::Internal);
    CHECK(events == 0);  // events are drained only after a successful save
}

TEST_CASE("UnlockAccount: save failure surfaces Internal and emits no events",
          "[application][auth][unlock][errors]") {
    Fixture f;
    f.seed_alice(/*locked=*/true);

    std::size_t events = 0;
    f.bus.subscribe([&](const domain::events::DomainEvent&) { ++events; });

    UnlockAccount uc{f.accounts, f.bus};
    auto r = uc.execute(f.alice_id, f.admin_id);

    REQUIRE_FALSE(r);
    CHECK(r.error().code() == core::StatusCode::Internal);
    CHECK(events == 0);
}
