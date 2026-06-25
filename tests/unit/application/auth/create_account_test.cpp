// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::auth::CreateAccount`. Exercises the use-case
// against the in-memory port adapters defined in `infra/`.

#include <catch2/catch_test_macros.hpp>

#include "application/auth/create_account.hpp"
#include "core/clock.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/ip_address.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"
#include "infra/inmemory/account_repository.hpp"
#include "infra/inmemory/event_bus.hpp"
#include "infra/inmemory/ip_ban_repository.hpp"

namespace {

using namespace pvpgn;
using application::auth::CreateAccount;
using application::auth::CreateAccountError;
using application::auth::CreateAccountRequest;

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
    infra::inmemory::InMemoryIpBanRepository ip_bans;
    infra::inmemory::InMemoryEventBus bus;
    core::ManualClock clock{core::SystemTime{}};

    domain::ClientTag star_tag = domain::ClientTag::parse("STAR").value();
    domain::IpAddress valid_ip;
    domain::BNHash password = make_hash(0xAA);

    CreateAccount make_use_case() {
        return CreateAccount{accounts, ip_bans, bus, clock};
    }

    CreateAccountRequest make_request(std::string_view username) {
        return CreateAccountRequest{
            make_name(username),
            password,
            "",  // no email
            domain::Locale{},
            star_tag,
            valid_ip};
    }
};

}  // namespace

TEST_CASE("CreateAccount: creates a new account with valid inputs",
          "[application][auth][create]") {
    Fixture f;
    auto uc = f.make_use_case();

    auto r = uc.execute(f.make_request("Alice"));

    REQUIRE(r);
    auto id = r.value();
    REQUIRE(id.value() != 0);

    // Verify account was saved
    auto found = f.accounts.find_by_name(make_name("Alice"));
    REQUIRE(found);
    REQUIRE(found.value().name() == make_name("Alice"));
}

TEST_CASE("CreateAccount: rejects username already taken",
          "[application][auth][create]") {
    Fixture f;
    auto uc = f.make_use_case();

    // Create first account
    auto r1 = uc.execute(f.make_request("Alice"));
    REQUIRE(r1);

    // Attempt to create same username
    auto r2 = uc.execute(f.make_request("Alice"));
    REQUIRE_FALSE(r2);
    REQUIRE(r2.error() == CreateAccountError::UsernameTaken);
}

TEST_CASE("CreateAccount: rejects username too short",
          "[application][auth][create]") {
    // Username too short - UserName::parse will reject this
    // This test verifies that the domain layer enforces minimum length
    auto r = domain::UserName::parse("A");  // 1 char
    REQUIRE_FALSE(r);
}

TEST_CASE("CreateAccount: rejects username too long",
          "[application][auth][create]") {
    // 16 characters (exceeds max of 15)
    // UserName::parse will reject this
    auto r = domain::UserName::parse("1234567890123456");
    REQUIRE_FALSE(r);
}

TEST_CASE("CreateAccount: rejects invalid characters in username",
          "[application][auth][create]") {
    // Username with spaces/special chars not allowed
    // UserName::parse will reject this
    auto r = domain::UserName::parse("Alice@Test");
    REQUIRE_FALSE(r);
}

TEST_CASE("CreateAccount: allows valid characters in username",
          "[application][auth][create]") {
    Fixture f;
    auto uc = f.make_use_case();

    // Valid: alphanumeric plus the original allowed symbol set -_[] (max 15
    // chars), including a leading bracket (legacy clan-tag style name).
    auto r = uc.execute(f.make_request("[Cl]Alice_1-2"));

    REQUIRE(r);
}

TEST_CASE("CreateAccount: rejects request from banned IP",
          "[application][auth][create]") {
    Fixture f;
    auto uc = f.make_use_case();

    // Add IP ban
    auto ip = domain::IpAddress::parse("192.168.1.1").value();
    domain::moderation::IpBanEntry ban{
        ip, "Test ban", domain::AccountId{0},
        core::SystemTime{}, std::nullopt};
    auto ban_result = f.ip_bans.add_ban(ban);
    REQUIRE(ban_result);

    auto req = f.make_request("Alice");
    req.peer_ip = ip;

    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == CreateAccountError::IpBanned);
}

TEST_CASE("CreateAccount: assigns sequential ids starting at 1",
          "[application][auth][create]") {
    // Mirrors the original server's maxuserid+1 allocation: the first-ever
    // account gets uid 1, then 2, 3, ... (sequential, monotonic, never reused).
    Fixture f;
    auto uc = f.make_use_case();

    auto r1 = uc.execute(f.make_request("Alice"));
    auto r2 = uc.execute(f.make_request("Bob"));
    auto r3 = uc.execute(f.make_request("Carol"));

    REQUIRE(r1);
    REQUIRE(r2);
    REQUIRE(r3);

    REQUIRE(r1.value().value() == 1u);
    REQUIRE(r2.value().value() == 2u);
    REQUIRE(r3.value().value() == 3u);
}

TEST_CASE("CreateAccount: never overwrites a different-named account (id collision regression)",
          "[application][auth][create]") {
    // Regression for Finding F1: the old implementation derived the account id
    // from a 31-bit hash of the username, so two distinct usernames could
    // collide on the same id and the second create would SILENTLY OVERWRITE
    // the first account's row (both repos key on id). The username-taken check
    // can't catch this because the names differ. With sequential max+1
    // allocation, both accounts must persist with distinct ids and both must
    // remain findable by name.
    Fixture f;
    auto uc = f.make_use_case();

    auto r1 = uc.execute(f.make_request("Alice"));
    auto r2 = uc.execute(f.make_request("Bob"));
    REQUIRE(r1);
    REQUIRE(r2);

    // Distinct ids — no clobber.
    REQUIRE(r1.value() != r2.value());

    // Both accounts still exist and are independently retrievable by name.
    REQUIRE(f.accounts.size() == 2u);

    auto alice = f.accounts.find_by_name(make_name("Alice"));
    auto bob = f.accounts.find_by_name(make_name("Bob"));
    REQUIRE(alice);
    REQUIRE(bob);

    REQUIRE(alice.value().name() == make_name("Alice"));
    REQUIRE(bob.value().name() == make_name("Bob"));

    // The id reported on creation matches the persisted account (no in-place
    // overwrite of an existing, differently-named row).
    REQUIRE(alice.value().id() == r1.value());
    REQUIRE(bob.value().id() == r2.value());

    // And each id resolves back to the account that was created under it.
    auto by_id_alice = f.accounts.find_by_id(r1.value());
    auto by_id_bob = f.accounts.find_by_id(r2.value());
    REQUIRE(by_id_alice);
    REQUIRE(by_id_bob);
    REQUIRE(by_id_alice.value().name() == make_name("Alice"));
    REQUIRE(by_id_bob.value().name() == make_name("Bob"));
}

TEST_CASE("CreateAccount: id allocation continues after the current max",
          "[application][auth][create]") {
    // After accounts already exist (e.g. loaded from storage), a new account
    // must take max(existing id)+1, never reusing or colliding with an id in
    // use — even if intermediate accounts were removed.
    Fixture f;
    auto uc = f.make_use_case();

    auto r1 = uc.execute(f.make_request("Alice"));   // id 1
    auto r2 = uc.execute(f.make_request("Bob"));     // id 2
    auto r3 = uc.execute(f.make_request("Carol"));   // id 3
    REQUIRE(r1);
    REQUIRE(r2);
    REQUIRE(r3);

    // Remove the lowest id; the next allocation must still advance past the
    // current max (3), not refill the gap at 1.
    auto removed = f.accounts.remove(r1.value());
    REQUIRE(removed);

    auto r4 = uc.execute(f.make_request("Dave"));
    REQUIRE(r4);
    REQUIRE(r4.value().value() == 4u);
}

TEST_CASE("CreateAccount: publishes domain events",
          "[application][auth][create]") {
    Fixture f;
    auto uc = f.make_use_case();

    std::size_t event_count = 0;
    f.bus.subscribe([&](const domain::events::DomainEvent&) { ++event_count; });

    auto r = uc.execute(f.make_request("Alice"));

    REQUIRE(r);
    REQUIRE(event_count > 0);  // AccountCreated at least
}
