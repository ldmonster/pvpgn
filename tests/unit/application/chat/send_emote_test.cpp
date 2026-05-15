// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::chat::SendEmote`. Exercises the use-case
// against the in-memory port adapters.

#include <catch2/catch_test_macros.hpp>

#include "application/chat/send_emote.hpp"
#include "domain/chat/channel.hpp"
#include "domain/shared/chat_message.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "infra/storage/repository/channel_repository.hpp"

namespace {

using namespace pvpgn;
using application::chat::SendEmote;
using application::chat::SendEmoteRequest;
using application::chat::SendEmoteError;

domain::ChatMessage make_message(std::string_view text) {
    auto r = domain::ChatMessage::parse(text);
    if (!r) return domain::ChatMessage{};
    return r.value();
}

struct Fixture {
    infra::storage::InMemoryChannelRepository channels;
    domain::AccountId                         alice_id{1};
    domain::AccountId                         bob_id{2};
    domain::SessionId                         alice_session{100};
    domain::ChannelId                         channel_id{1};

    void setup_channel_with_members() {
        auto ch = domain::chat::Channel::create(
            channel_id, "TestChannel", domain::chat::ChannelPolicy{});
        ch.admit(alice_id, domain::ClientTag{});
        ch.admit(bob_id, domain::ClientTag{});
        channels.save(ch);
    }

    SendEmote make_use_case() {
        return SendEmote{
            std::make_shared<infra::storage::InMemoryChannelRepository>(
                channels),
            nullptr};  // Router not tested here
    }
};

}  // namespace

TEST_CASE("SendEmote: sending emote in a channel succeeds when account is member",
          "[application][chat][emote]") {
    Fixture f;
    f.setup_channel_with_members();

    auto uc = f.make_use_case();
    SendEmoteRequest req{
        .sender_id = f.alice_id,
        .channel_id = f.channel_id,
        .emote_text = make_message("dances"),
        .sender_session = f.alice_session,
    };
    auto r = uc.execute(req);

    REQUIRE(r);
}

TEST_CASE("SendEmote: emote fails when account not in channel",
          "[application][chat][emote]") {
    Fixture f;
    f.setup_channel_with_members();

    auto uc = f.make_use_case();
    SendEmoteRequest req{
        .sender_id = domain::AccountId{999},
        .channel_id = f.channel_id,
        .emote_text = make_message("waves"),
        .sender_session = f.alice_session,
    };
    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == SendEmoteError::NotInChannel);
}

TEST_CASE("SendEmote: emote fails when channel not found",
          "[application][chat][emote]") {
    Fixture f;
    f.setup_channel_with_members();

    auto uc = f.make_use_case();
    SendEmoteRequest req{
        .sender_id = f.alice_id,
        .channel_id = domain::ChannelId{999},
        .emote_text = make_message("bows"),
        .sender_session = f.alice_session,
    };
    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == SendEmoteError::ChannelNotFound);
}

TEST_CASE("SendEmote: result contains correct sender and emote text",
          "[application][chat][emote]") {
    Fixture f;
    f.setup_channel_with_members();

    auto uc = f.make_use_case();
    SendEmoteRequest req{
        .sender_id = f.alice_id,
        .channel_id = f.channel_id,
        .emote_text = make_message("laughs"),
        .sender_session = f.alice_session,
    };
    auto r = uc.execute(req);

    REQUIRE(r);
    REQUIRE(r.value().event.account_id == f.alice_id);
}
