// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::chat::KickFromChannel`. Exercises the use-case
// against the in-memory port adapters.

#include <catch2/catch_test_macros.hpp>

#include "application/chat/kick_from_channel.hpp"
#include "domain/chat/channel.hpp"
#include "domain/shared/ids.hpp"
#include "channel_repository.hpp"

namespace {

using namespace pvpgn;
using application::chat::KickFromChannel;
using application::chat::KickFromChannelRequest;
using application::chat::KickFromChannelError;

struct Fixture {
    infra::storage::InMemoryChannelRepository channels;
    domain::AccountId                         alice_id{1};
    domain::AccountId                         bob_id{2};
    domain::AccountId                         charlie_id{3};
    domain::ChannelId                         channel_id{1};

    void setup_channel_with_members() {
        auto ch = domain::chat::Channel::create(
            channel_id, "TestChannel", domain::chat::ChannelPolicy{});
        auto star_tag = domain::ClientTag::parse("STAR").value();
        (void)ch.admit(alice_id, star_tag);
        (void)ch.admit(bob_id, star_tag);
        (void)ch.admit(charlie_id, star_tag);
        REQUIRE(channels.save(ch));
    }

    KickFromChannel make_use_case() {
        return KickFromChannel{
            std::shared_ptr<infra::storage::InMemoryChannelRepository>(
                &channels, [](auto*) {}),
            nullptr,  // Account repository not tested here
            nullptr   // Message router not tested here
        };
    }
};

}  // namespace

TEST_CASE("KickFromChannel: kicking a member succeeds",
          "[application][chat][kick]") {
    Fixture f;
    f.setup_channel_with_members();

    auto uc = f.make_use_case();
    KickFromChannelRequest req{
        .kicker_id = f.alice_id,
        .target_id = f.bob_id,
        .channel_id = f.channel_id,
        .reason = "spam",
    };
    auto r = uc.execute(req);

    REQUIRE(r);
}

TEST_CASE("KickFromChannel: cannot kick self",
          "[application][chat][kick]") {
    Fixture f;
    f.setup_channel_with_members();

    auto uc = f.make_use_case();
    KickFromChannelRequest req{
        .kicker_id = f.alice_id,
        .target_id = f.alice_id,
        .channel_id = f.channel_id,
        .reason = "whatever",
    };
    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == KickFromChannelError::CannotKickSelf);
}

TEST_CASE("KickFromChannel: fails when target not in channel",
          "[application][chat][kick]") {
    Fixture f;
    f.setup_channel_with_members();

    auto uc = f.make_use_case();
    KickFromChannelRequest req{
        .kicker_id = f.alice_id,
        .target_id = domain::AccountId{999},
        .channel_id = f.channel_id,
        .reason = "not_here",
    };
    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == KickFromChannelError::TargetNotInChannel);
}

TEST_CASE("KickFromChannel: fails when channel not found",
          "[application][chat][kick]") {
    Fixture f;
    f.setup_channel_with_members();

    auto uc = f.make_use_case();
    KickFromChannelRequest req{
        .kicker_id = f.alice_id,
        .target_id = f.bob_id,
        .channel_id = domain::ChannelId{999},
        .reason = "gone",
    };
    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == KickFromChannelError::ChannelNotFound);
}

TEST_CASE("KickFromChannel: fails when kicker not in channel",
          "[application][chat][kick]") {
    Fixture f;
    f.setup_channel_with_members();

    auto uc = f.make_use_case();
    KickFromChannelRequest req{
        .kicker_id = domain::AccountId{999},
        .target_id = f.bob_id,
        .channel_id = f.channel_id,
        .reason = "not_member",
    };
    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == KickFromChannelError::TargetNotInChannel);
}
