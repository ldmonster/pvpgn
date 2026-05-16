// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::chat::SetChannelTopic`. Exercises the use-case
// against the in-memory port adapters.

#include <catch2/catch_test_macros.hpp>

#include "application/chat/set_channel_topic.hpp"
#include "domain/chat/channel.hpp"
#include "domain/shared/ids.hpp"
#include "channel_repository.hpp"

namespace {

using namespace pvpgn;
using application::chat::SetChannelTopic;
using application::chat::SetChannelTopicRequest;
using application::chat::SetChannelTopicError;

struct Fixture {
    infra::storage::InMemoryChannelRepository channels;
    domain::AccountId                         alice_id{1};
    domain::ChannelId                         channel_id{1};

    void setup_channel_with_member() {
        auto ch = domain::chat::Channel::create(
            channel_id, "TestChannel", domain::chat::ChannelPolicy{});
        auto star_tag = domain::ClientTag::parse("STAR").value();
        (void)ch.admit(alice_id, star_tag);
        REQUIRE(channels.save(ch));
    }

    SetChannelTopic make_use_case() {
        return SetChannelTopic{
            std::shared_ptr<infra::storage::InMemoryChannelRepository>(
                &channels, [](auto*) {}),
            nullptr  // Message router not tested here
        };
    }
};

}  // namespace

TEST_CASE("SetChannelTopic: setting topic succeeds when account is member",
          "[application][chat][topic]") {
    Fixture f;
    f.setup_channel_with_member();

    auto uc = f.make_use_case();
    SetChannelTopicRequest req{
        .setter_id = f.alice_id,
        .channel_id = f.channel_id,
        .new_topic = "Welcome to our channel!",
    };
    auto r = uc.execute(req);

    REQUIRE(r);
}

TEST_CASE("SetChannelTopic: fails when account not in channel",
          "[application][chat][topic]") {
    Fixture f;
    f.setup_channel_with_member();

    auto uc = f.make_use_case();
    SetChannelTopicRequest req{
        .setter_id = domain::AccountId{999},
        .channel_id = f.channel_id,
        .new_topic = "New topic",
    };
    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == SetChannelTopicError::InsufficientPermissions);
}

TEST_CASE("SetChannelTopic: fails when channel not found",
          "[application][chat][topic]") {
    Fixture f;
    f.setup_channel_with_member();

    auto uc = f.make_use_case();
    SetChannelTopicRequest req{
        .setter_id = f.alice_id,
        .channel_id = domain::ChannelId{999},
        .new_topic = "Missing channel",
    };
    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == SetChannelTopicError::ChannelNotFound);
}

TEST_CASE("SetChannelTopic: fails when topic too long",
          "[application][chat][topic]") {
    Fixture f;
    f.setup_channel_with_member();

    auto uc = f.make_use_case();
    std::string long_topic(300, 'a');  // Longer than 255 chars
    SetChannelTopicRequest req{
        .setter_id = f.alice_id,
        .channel_id = f.channel_id,
        .new_topic = long_topic,
    };
    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == SetChannelTopicError::TopicTooLong);
}

TEST_CASE("SetChannelTopic: can set empty topic",
          "[application][chat][topic]") {
    Fixture f;
    f.setup_channel_with_member();

    auto uc = f.make_use_case();
    SetChannelTopicRequest req{
        .setter_id = f.alice_id,
        .channel_id = f.channel_id,
        .new_topic = "",
    };
    auto r = uc.execute(req);

    REQUIRE(r);
}

TEST_CASE("SetChannelTopic: can set max-length topic",
          "[application][chat][topic]") {
    Fixture f;
    f.setup_channel_with_member();

    auto uc = f.make_use_case();
    std::string max_topic(255, 'x');  // Exactly 255 chars
    SetChannelTopicRequest req{
        .setter_id = f.alice_id,
        .channel_id = f.channel_id,
        .new_topic = max_topic,
    };
    auto r = uc.execute(req);

    REQUIRE(r);
}
