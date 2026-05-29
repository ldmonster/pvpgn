// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::chat::LeaveChannel`. Exercises the use-case
// against the in-memory port adapters.

#include <catch2/catch_test_macros.hpp>

#include "application/chat/leave_channel.hpp"
#include "domain/chat/channel.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "infra/inmemory/session_registry.hpp"
#include "channel_repository.hpp"

namespace {

using namespace pvpgn;
using application::chat::LeaveChannel;
using application::chat::LeaveChannelError;

struct Fixture {
    infra::storage::InMemoryChannelRepository channels;
    infra::inmemory::InMemorySessionRegistry sessions;
    domain::AccountId alice_id{1};
    domain::AccountId bob_id{2};
    domain::ChannelId channel_id{1};

    void setup_channel_with_members() {
        auto ch = domain::chat::Channel::create(
            channel_id, "TestChannel", domain::chat::ChannelPolicy{});
        auto star_tag = domain::ClientTag::parse("STAR").value();
        (void)ch.admit(alice_id, star_tag);
        (void)ch.admit(bob_id, star_tag);
        REQUIRE(channels.save(ch));
        // Register sessions so members_to_notify is populated
        (void)sessions.attach(domain::SessionId{101}, alice_id);
        (void)sessions.attach(domain::SessionId{102}, bob_id);
    }

    void setup_permanent_channel_with_members() {
        domain::chat::ChannelPolicy policy;
        policy.flags.set(domain::chat::ChannelFlag::Permanent);
        auto ch = domain::chat::Channel::create(
            channel_id, "TestChannel", policy);
        auto star_tag = domain::ClientTag::parse("STAR").value();
        (void)ch.admit(alice_id, star_tag);
        (void)ch.admit(bob_id, star_tag);
        REQUIRE(channels.save(ch));
        (void)sessions.attach(domain::SessionId{101}, alice_id);
        (void)sessions.attach(domain::SessionId{102}, bob_id);
    }

    LeaveChannel make_use_case() {
        return LeaveChannel{channels, sessions};
    }
};

}  // namespace

TEST_CASE("LeaveChannel: leaving a channel removes the account from members",
          "[application][chat][leave]") {
    Fixture f;
    f.setup_channel_with_members();

    auto uc = f.make_use_case();
    auto r = uc.execute(f.channel_id, f.alice_id);

    REQUIRE(r);
    // Verify Alice was removed by checking the channel state
    auto ch = f.channels.find_by_id(f.channel_id);
    REQUIRE(ch);
    REQUIRE(ch.value().member_count() == 1);  // Only Bob remains
}

TEST_CASE("LeaveChannel: empty non-permanent channel is removed from repository",
          "[application][chat][leave]") {
    Fixture f;
    f.setup_channel_with_members();
    REQUIRE(f.channels.size() == 1);

    auto uc = f.make_use_case();
    // Bob leaves (Alice already left)
    (void)uc.execute(f.channel_id, f.alice_id);
    auto r2 = uc.execute(f.channel_id, f.bob_id);

    REQUIRE(r2);
    REQUIRE(r2.value().channel_deleted);
    REQUIRE(f.channels.size() == 0);  // Channel should be removed
}

TEST_CASE("LeaveChannel: permanent channel persists even when empty",
          "[application][chat][leave]") {
    Fixture f;
    f.setup_permanent_channel_with_members();
    REQUIRE(f.channels.size() == 1);

    auto uc = f.make_use_case();
    (void)uc.execute(f.channel_id, f.alice_id);
    (void)uc.execute(f.channel_id, f.bob_id);

    // Channel should still exist since it's permanent
    REQUIRE(f.channels.size() == 1);
    auto ch = f.channels.find_by_id(f.channel_id);
    REQUIRE(ch);
}

TEST_CASE("LeaveChannel: leaving when not a member returns NotInChannel error",
          "[application][chat][leave]") {
    Fixture f;
    f.setup_channel_with_members();

    auto uc = f.make_use_case();
    auto r = uc.execute(f.channel_id, domain::AccountId{999});

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == LeaveChannelError::NotInChannel);
}

TEST_CASE("LeaveChannel: channel not found returns error",
          "[application][chat][leave]") {
    Fixture f;

    auto uc = f.make_use_case();
    auto r = uc.execute(domain::ChannelId{999}, f.alice_id);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == LeaveChannelError::ChannelNotFound);
}
