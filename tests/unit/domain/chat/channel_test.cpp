// SPDX-License-Identifier: GPL-2.0-or-later
#include <variant>

#include <catch2/catch_test_macros.hpp>

#include "domain/chat/channel.hpp"

using namespace pvpgn;
using domain::AccountId;
using domain::ChannelId;
using domain::ChatMessage;
using domain::ClientTag;
using domain::chat::Channel;
using domain::chat::ChannelFlag;
using domain::chat::ChannelPolicy;

namespace {
const ClientTag kStar = ClientTag::parse("STAR").value();
const ClientTag kD2DV = ClientTag::parse("D2DV").value();

Channel make_channel(std::uint32_t max = 0, ClientTag client = ClientTag{}) {
    ChannelPolicy p{};
    p.max_members = max;
    p.client = client;
    return Channel::create(ChannelId{1}, "Lobby", p);
}
}  // namespace

TEST_CASE("Channel: admit emits ChannelJoined and increments member count",
          "[domain][chat]") {
    auto c = make_channel();
    REQUIRE(c.admit(AccountId{10}, kStar) == Channel::JoinOutcome::Accepted);
    REQUIRE(c.member_count() == 1);
    auto evs = c.drain_events();
    REQUIRE(evs.size() == 1);
    REQUIRE(std::holds_alternative<domain::events::ChannelJoined>(evs[0]));
}

TEST_CASE("Channel: admit is idempotent (no duplicate event on rejoin)",
          "[domain][chat]") {
    auto c = make_channel();
    (void)c.admit(AccountId{10}, kStar);
    (void)c.drain_events();
    REQUIRE(c.admit(AccountId{10}, kStar) == Channel::JoinOutcome::Accepted);
    REQUIRE(c.member_count() == 1);
    REQUIRE(c.drain_events().empty());
}

TEST_CASE("Channel: full-capacity rejects further admits",
          "[domain][chat]") {
    auto c = make_channel(2);
    REQUIRE(c.admit(AccountId{1}, kStar) == Channel::JoinOutcome::Accepted);
    REQUIRE(c.admit(AccountId{2}, kStar) == Channel::JoinOutcome::Accepted);
    REQUIRE(c.admit(AccountId{3}, kStar) == Channel::JoinOutcome::Full);
    REQUIRE(c.member_count() == 2);
}

TEST_CASE("Channel: wrong-client-tag is rejected when policy restricts",
          "[domain][chat]") {
    auto c = make_channel(0, kStar);
    REQUIRE(c.admit(AccountId{1}, kD2DV) == Channel::JoinOutcome::WrongClientTag);
    REQUIRE(c.admit(AccountId{1}, kStar) == Channel::JoinOutcome::Accepted);
}

TEST_CASE("Channel: locked flag rejects every admit",
          "[domain][chat]") {
    auto p = ChannelPolicy{};
    p.flags.set(ChannelFlag::Locked);
    auto c = Channel::create(ChannelId{1}, "Closed", p);
    REQUIRE(c.admit(AccountId{1}, kStar) == Channel::JoinOutcome::Locked);
}

TEST_CASE("Channel: post requires membership", "[domain][chat]") {
    auto c = make_channel();
    auto msg = ChatMessage::create("hi").value();
    REQUIRE_FALSE(c.post(AccountId{1}, msg));
    (void)c.admit(AccountId{1}, kStar);
    (void)c.drain_events();
    REQUIRE(c.post(AccountId{1}, msg));
    auto evs = c.drain_events();
    REQUIRE(evs.size() == 1);
    REQUIRE(std::holds_alternative<domain::events::ChannelMessageSent>(evs[0]));
}

TEST_CASE("Channel: kick removes target and bans them from rejoining",
          "[domain][chat]") {
    auto c = make_channel();
    (void)c.admit(AccountId{1}, kStar);  // moderator
    (void)c.admit(AccountId{2}, kStar);  // target
    (void)c.drain_events();
    REQUIRE(c.kick(AccountId{1}, AccountId{2}));
    REQUIRE_FALSE(c.contains(AccountId{2}));
    REQUIRE(c.admit(AccountId{2}, kStar) == Channel::JoinOutcome::Banned);
}

TEST_CASE("Channel: set_topic emits ChannelTopicChanged when moderator is a member",
          "[domain][chat]") {
    auto c = make_channel();
    (void)c.admit(AccountId{1}, kStar);
    (void)c.drain_events();
    c.set_topic(AccountId{1}, "be excellent");
    auto evs = c.drain_events();
    REQUIRE(evs.size() == 1);
    REQUIRE(std::get<domain::events::ChannelTopicChanged>(evs[0]).topic == "be excellent");
}

TEST_CASE("ChatMessage::create rejects empty and control-char payloads",
          "[domain][chat][value]") {
    REQUIRE_FALSE(ChatMessage::create("").has_value());
    REQUIRE_FALSE(ChatMessage::create(std::string("a\nb")).has_value());
    REQUIRE(ChatMessage::create("ok").has_value());
}
