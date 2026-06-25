// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::chat::JoinChannel`. Exercises the use-case
// against the in-memory port adapters defined in `infra/`.

#include <catch2/catch_test_macros.hpp>

#include "application/chat/join_channel.hpp"
#include "domain/chat/channel.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/user_name.hpp"
#include "infra/inmemory/account_repository.hpp"
#include "infra/inmemory/session_registry.hpp"
#include "channel_repository.hpp"

namespace {

using namespace pvpgn;
using application::chat::JoinChannel;
using application::chat::JoinChannelError;

domain::UserName make_name(std::string_view s) {
    auto r = domain::UserName::parse(s);
    REQUIRE(r);
    return r.value();
}

domain::BNHash make_hash(std::uint8_t fill) {
    domain::BNHash::Bytes b{};
    b.fill(fill);
    return domain::BNHash{b};
}

struct Fixture {
    infra::inmemory::InMemoryAccountRepository accounts;
    infra::storage::InMemoryChannelRepository channels;
    infra::inmemory::InMemorySessionRegistry sessions;

    domain::AccountId alice_id{1};
    domain::AccountId bob_id{2};
    domain::ClientTag star_tag = domain::ClientTag::parse("STAR").value();

    void seed_account(domain::AccountId id, std::string_view name) {
        auto a = domain::identity::Account::create(
            id, make_name(name), make_hash(0xAA),
            domain::Locale{}).value();
        (void)a.drain_events();
        REQUIRE(accounts.save(a));
    }

    JoinChannel make_use_case() {
        return JoinChannel{channels, accounts, sessions};
    }
};

}  // namespace

TEST_CASE("JoinChannel: joining a non-existent channel creates it",
          "[application][chat][join]") {
    Fixture f;
    f.seed_account(f.alice_id, "Alice");

    auto uc = f.make_use_case();
    auto r = uc.execute(f.alice_id, "TestChannel", f.star_tag);

    REQUIRE(r);
    REQUIRE(r.value().channel.name() == "TestChannel");
    REQUIRE(f.channels.size() == 1);
}

TEST_CASE("JoinChannel: joining an existing channel adds account to members",
          "[application][chat][join]") {
    Fixture f;
    f.seed_account(f.alice_id, "Alice");
    f.seed_account(f.bob_id, "Bob");

    // Alice creates the channel
    auto uc = f.make_use_case();
    auto r1 = uc.execute(f.alice_id, "TestChannel", f.star_tag);
    REQUIRE(r1);

    // Bob joins the same channel
    auto r2 = uc.execute(f.bob_id, "TestChannel", f.star_tag);
    REQUIRE(r2);
    
    // Bob should be in the result
    REQUIRE(r2.value().channel.member_count() >= 1);
}

TEST_CASE("JoinChannel: result contains channel snapshot with member list",
          "[application][chat][join]") {
    Fixture f;
    f.seed_account(f.alice_id, "Alice");

    auto uc = f.make_use_case();
    auto r = uc.execute(f.alice_id, "TestChannel", f.star_tag);

    REQUIRE(r);
    REQUIRE(r.value().channel.id().value() != 0);  // Has valid ID
    REQUIRE(r.value().channel.member_count() > 0);
}

TEST_CASE("JoinChannel: channel name lookup is case-insensitive (F6)",
          "[application][chat][join]") {
    // Regression for channel-routing F6: the original server matches channel
    // names with strcasecmp, so joining "War3" then "war3" must land in the
    // SAME channel instead of spawning a duplicate. The stored display name
    // must keep its original ("War3") casing.
    Fixture f;
    f.seed_account(f.alice_id, "Alice");
    f.seed_account(f.bob_id, "Bob");

    auto uc = f.make_use_case();

    // Alice creates "War3".
    auto r1 = uc.execute(f.alice_id, "War3", f.star_tag);
    REQUIRE(r1);
    REQUIRE(r1.value().channel.name() == "War3");
    const auto created_id = r1.value().channel.id().value();

    // Bob joins "war3" (different case) — must resolve to the same channel.
    auto r2 = uc.execute(f.bob_id, "war3", f.star_tag);
    REQUIRE(r2);

    // Same channel id, no duplicate created.
    REQUIRE(r2.value().channel.id().value() == created_id);
    REQUIRE(f.channels.size() == 1);

    // Display name preserves the original "War3" casing (not lowercased).
    REQUIRE(r2.value().channel.name() == "War3");

    // Lookups via either casing return the same id.
    auto a = f.channels.find_by_name("WAR3");
    auto b = f.channels.find_by_name("war3");
    REQUIRE(a);
    REQUIRE(b);
    REQUIRE(a.value().id().value() == created_id);
    REQUIRE(b.value().id().value() == created_id);
}

TEST_CASE("JoinChannel: account not found returns error",
          "[application][chat][join]") {
    Fixture f;
    // Don't seed any account

    auto uc = f.make_use_case();
    auto r = uc.execute(domain::AccountId{999}, "TestChannel", f.star_tag);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == JoinChannelError::AccountNotFound);
}

TEST_CASE("JoinChannel: invalid channel name returns error",
          "[application][chat][join]") {
    Fixture f;
    f.seed_account(f.alice_id, "Alice");

    auto uc = f.make_use_case();
    // Channel names with invalid characters should fail
    auto r = uc.execute(f.alice_id, "\x00\x01", f.star_tag);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == JoinChannelError::InvalidChannelName);
}
