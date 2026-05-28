// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::auth::ListSessions`. Exercises the use-case
// against the in-memory port adapters defined in `infra/inmemory/`.

#include <algorithm>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "application/auth/list_sessions.hpp"
#include "domain/identity/account.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"
#include "infra/inmemory/account_repository.hpp"
#include "infra/inmemory/session_registry.hpp"

namespace {

using namespace pvpgn;
using application::auth::ListSessions;
using application::auth::SessionInfo;

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

    domain::AccountId alice_id{1};
    domain::AccountId bob_id{2};
    domain::SessionId alice_session{10};
    domain::SessionId bob_session{20};

    void seed_alice() {
        auto a = domain::identity::Account::create(
            alice_id, make_name("Alice"), make_hash(0xAA),
            domain::Locale{}).value();
        (void)a.drain_events();
        REQUIRE(accounts.save(a));
    }

    void seed_bob() {
        auto b = domain::identity::Account::create(
            bob_id, make_name("Bob"), make_hash(0xBB),
            domain::Locale{}).value();
        (void)b.drain_events();
        REQUIRE(accounts.save(b));
    }

    ListSessions make_use_case() {
        return ListSessions{sessions, accounts};
    }
};

}  // namespace

TEST_CASE("ListSessions: no active sessions returns empty vector",
          "[application][auth][list_sessions]") {
    Fixture f;
    f.seed_alice();

    auto uc = f.make_use_case();
    auto r  = uc.execute();

    REQUIRE(r);
    REQUIRE(r.value().empty());
}

TEST_CASE("ListSessions: one active session returns correct SessionInfo",
          "[application][auth][list_sessions]") {
    Fixture f;
    f.seed_alice();
    REQUIRE(f.sessions.attach(f.alice_session, f.alice_id));

    auto uc = f.make_use_case();
    auto r  = uc.execute();

    REQUIRE(r);
    REQUIRE(r.value().size() == 1u);
    REQUIRE(r.value()[0].account_id   == f.alice_id);
    REQUIRE(r.value()[0].account_name == "alice");
}

TEST_CASE("ListSessions: multiple sessions returns all",
          "[application][auth][list_sessions]") {
    Fixture f;
    f.seed_alice();
    f.seed_bob();
    REQUIRE(f.sessions.attach(f.alice_session, f.alice_id));
    REQUIRE(f.sessions.attach(f.bob_session,   f.bob_id));

    auto uc = f.make_use_case();
    auto r  = uc.execute();

    REQUIRE(r);
    REQUIRE(r.value().size() == 2u);

    // Order is not guaranteed — check by searching.
    const auto& infos = r.value();
    auto has_alice = std::any_of(infos.begin(), infos.end(),
        [&](const SessionInfo& s) {
            return s.account_id == f.alice_id && s.account_name == "alice";
        });
    auto has_bob = std::any_of(infos.begin(), infos.end(),
        [&](const SessionInfo& s) {
            return s.account_id == f.bob_id && s.account_name == "bob";
        });

    REQUIRE(has_alice);
    REQUIRE(has_bob);
}
