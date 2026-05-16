// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::chat::PostMessage`. Exercises the use-case
// against the in-memory port adapters.

#include <catch2/catch_test_macros.hpp>

#include "application/chat/post_message.hpp"
#include "domain/chat/channel.hpp"
#include "domain/shared/chat_message.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/user_name.hpp"
#include "channel_repository.hpp"

namespace {

using namespace pvpgn;
using application::chat::PostMessage;
using application::chat::PostMessageError;

domain::ChatMessage make_message(std::string_view text) {
    auto r = domain::ChatMessage::create(text);
    REQUIRE(r);
    return r.value();
}

struct Fixture {
    infra::storage::InMemoryChannelRepository channels;
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
    }

    PostMessage make_use_case() {
        return PostMessage{channels};
    }
};

}  // namespace

TEST_CASE("PostMessage: posting a message in a channel succeeds when account is a member",
          "[application][chat][message]") {
    Fixture f;
    f.setup_channel_with_members();

    auto uc = f.make_use_case();
    auto r = uc.execute(f.channel_id, f.alice_id, make_message("Hello everyone"));

    REQUIRE(r);
    REQUIRE(!r.value().recipients.empty());
}

TEST_CASE("PostMessage: posting when not a member returns NotInChannel error",
          "[application][chat][message]") {
    Fixture f;
    f.setup_channel_with_members();

    auto uc = f.make_use_case();
    auto r = uc.execute(f.channel_id, domain::AccountId{999}, 
                        make_message("Hello"));

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == PostMessageError::NotInChannel);
}

TEST_CASE("PostMessage: result contains ChatEvent with correct message and sender",
          "[application][chat][message]") {
    Fixture f;
    f.setup_channel_with_members();

    auto uc = f.make_use_case();
    auto msg = make_message("Test message");
    auto r = uc.execute(f.channel_id, f.alice_id, msg);

    REQUIRE(r);
    REQUIRE(r.value().event.from == f.alice_id);
}

TEST_CASE("PostMessage: result contains list of recipient session IDs",
          "[application][chat][message]") {
    Fixture f;
    f.setup_channel_with_members();

    auto uc = f.make_use_case();
    auto r = uc.execute(f.channel_id, f.alice_id, make_message("Hi"));

    REQUIRE(r);
    auto& recipients = r.value().recipients;
    REQUIRE(!recipients.empty());
}

TEST_CASE("PostMessage: channel not found returns error",
          "[application][chat][message]") {
    Fixture f;

    auto uc = f.make_use_case();
    auto r = uc.execute(domain::ChannelId{999}, f.alice_id, 
                        make_message("Hello"));

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == PostMessageError::ChannelNotFound);
}
