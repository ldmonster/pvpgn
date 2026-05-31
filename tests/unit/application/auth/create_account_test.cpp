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

    // Valid: alphanumeric, dots, dashes, underscores (max 15 chars)
    auto r = uc.execute(f.make_request("Alice.123-test"));

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
