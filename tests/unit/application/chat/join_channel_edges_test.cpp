// SPDX-License-Identifier: GPL-2.0-or-later
//
// Edge/error-branch tests for `application::chat::JoinChannel`.
// Exercises the domain-outcome mapping arms (Locked / Banned /
// WrongClientTag / Full) that the happy-path tests in
// join_channel_test.cpp do not reach. Channels are pre-seeded with a
// non-zero ID so the in-memory repository preserves their policy and
// banlist (the auto-ID path rebuilds with an empty policy).

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
    domain::ChannelId channel_id{10};
    domain::ClientTag star_tag = domain::ClientTag::parse("STAR").value();
    domain::ClientTag war3_tag = domain::ClientTag::parse("WAR3").value();

    void seed_account(domain::AccountId id, std::string_view name) {
        auto a = domain::identity::Account::create(
            id, make_name(name), make_hash(0xAA),
            domain::Locale{}).value();
        (void)a.drain_events();
        REQUIRE(accounts.save(a));
    }

    // Seed a channel with an explicit non-zero ID so the repository keeps
    // the supplied policy/members verbatim.
    void seed_channel(domain::chat::Channel ch) {
        (void)ch.drain_events();
        REQUIRE(channels.save(ch));
    }

    JoinChannel make_use_case() {
        return JoinChannel{channels, accounts, sessions};
    }
};

}  // namespace

TEST_CASE("JoinChannel: joining a locked channel returns Locked",
          "[application][chat][join]") {
    Fixture f;
    f.seed_account(f.alice_id, "Alice");

    domain::chat::ChannelPolicy policy{};
    policy.flags.set(domain::chat::ChannelFlag::Locked);
    f.seed_channel(domain::chat::Channel::create(f.channel_id, "Locked", policy));

    auto uc = f.make_use_case();
    auto r = uc.execute(f.alice_id, "Locked", f.star_tag);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == JoinChannelError::Locked);
}

TEST_CASE("JoinChannel: joining a channel that banned the account returns Banned",
          "[application][chat][join]") {
    Fixture f;
    f.seed_account(f.alice_id, "Alice");
    f.seed_account(f.bob_id, "Bob");

    // Bob is a member who kicks Alice -> Alice lands on the banlist.
    auto ch = domain::chat::Channel::create(
        f.channel_id, "Banned", domain::chat::ChannelPolicy{});
    (void)ch.admit(f.bob_id, f.star_tag);
    (void)ch.kick(f.bob_id, f.alice_id);  // adds alice to banlist
    f.seed_channel(ch);

    auto uc = f.make_use_case();
    auto r = uc.execute(f.alice_id, "Banned", f.star_tag);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == JoinChannelError::Banned);
}

TEST_CASE("JoinChannel: joining with the wrong client tag returns WrongClientTag",
          "[application][chat][join]") {
    Fixture f;
    f.seed_account(f.alice_id, "Alice");

    domain::chat::ChannelPolicy policy{};
    policy.client = f.war3_tag;  // channel restricted to WAR3 clients
    f.seed_channel(domain::chat::Channel::create(f.channel_id, "War3Only", policy));

    auto uc = f.make_use_case();
    auto r = uc.execute(f.alice_id, "War3Only", f.star_tag);  // STAR client

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == JoinChannelError::WrongClientTag);
}

TEST_CASE("JoinChannel: joining a full channel returns Full",
          "[application][chat][join]") {
    Fixture f;
    f.seed_account(f.alice_id, "Alice");
    f.seed_account(f.bob_id, "Bob");

    domain::chat::ChannelPolicy policy{};
    policy.max_members = 1;  // capacity of one
    auto ch = domain::chat::Channel::create(f.channel_id, "Full", policy);
    (void)ch.admit(f.bob_id, f.star_tag);  // fill the single slot
    f.seed_channel(ch);

    auto uc = f.make_use_case();
    auto r = uc.execute(f.alice_id, "Full", f.star_tag);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == JoinChannelError::Full);
}

TEST_CASE("JoinChannel: re-joining a channel already a member of succeeds (idempotent)",
          "[application][chat][join]") {
    Fixture f;
    f.seed_account(f.alice_id, "Alice");

    auto ch = domain::chat::Channel::create(
        f.channel_id, "Existing", domain::chat::ChannelPolicy{});
    (void)ch.admit(f.alice_id, f.star_tag);  // alice already a member
    f.seed_channel(ch);

    auto uc = f.make_use_case();
    auto r = uc.execute(f.alice_id, "Existing", f.star_tag);

    REQUIRE(r);
    REQUIRE(r.value().channel.member_count() == 1);
}
