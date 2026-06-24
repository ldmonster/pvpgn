// SPDX-License-Identifier: GPL-2.0-or-later
//
// Edge/variant tests for compose_chat_event. Covers arms not
// exercised by chat_event_compose_test.cpp:
//   * Whisper with me_present == true (the non-fallback username branch)
//   * Join happy path (event/flags/text wiring)
//   * me==NULL rejection for the remaining message types
//     (AddUser, UserFlags, WhisperAck, FriendWhisperAck, Channel,
//      ChannelDoesNotExist, Emote)

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

#include "application/chat/chat_event_compose.hpp"
#include "core/error.hpp"
#include "protocol/bnet/chat_wire_types.hpp"
#include "protocol/bnet/messages.hpp"

namespace ac = pvpgn::application::chat;
namespace pb = pvpgn::protocol::bnet::chat;
using pvpgn::core::StatusCode;

namespace {

ac::ComposeRequest req_with_me(ac::LegacyMessageType t) {
    ac::ComposeRequest r;
    r.type         = t;
    r.me_present   = true;
    r.me_flags     = 0x12u;
    r.me_latency   = 50u;
    r.dstflags     = 0x80u;
    r.chatcharname = "Bob";
    r.chatname     = "Bob";
    r.playerinfo   = "1RAW";
    r.text         = "hi";
    return r;
}

}  // namespace

TEST_CASE("compose Whisper: me present uses chatcharname and combined flags",
          "[application_chat]") {
    auto r = req_with_me(ac::LegacyMessageType::Whisper);
    r.servername = "PvPGN";  // should be ignored when me is present
    auto res = ac::compose_chat_event(r);
    REQUIRE(res.has_value());
    auto const& ev = res.value();
    CHECK(ev.event_id == pb::kServerMessageTypeWhisper);
    CHECK(ev.flags    == (0x12u | 0x80u));
    CHECK(ev.ping_ms  == 50u);
    CHECK(ev.username == "Bob");
    CHECK(ev.text     == "hi");
}

TEST_CASE("compose Join: happy path wires username and playerinfo",
          "[application_chat]") {
    auto res = ac::compose_chat_event(req_with_me(ac::LegacyMessageType::Join));
    REQUIRE(res.has_value());
    auto const& ev = res.value();
    CHECK(ev.event_id == pb::kServerMessageTypeJoin);
    CHECK(ev.flags    == (0x12u | 0x80u));
    CHECK(ev.ping_ms  == 50u);
    CHECK(ev.username == "Bob");
    CHECK(ev.text     == "1RAW");
}

TEST_CASE("compose AddUser rejects me==NULL", "[application_chat]") {
    auto r = req_with_me(ac::LegacyMessageType::AddUser);
    r.me_present = false;
    auto res = ac::compose_chat_event(r);
    REQUIRE_FALSE(res.has_value());
    CHECK(res.error().code() == StatusCode::NotFound);
}

TEST_CASE("compose Join rejects me==NULL", "[application_chat]") {
    auto r = req_with_me(ac::LegacyMessageType::Join);
    r.me_present = false;
    auto res = ac::compose_chat_event(r);
    REQUIRE_FALSE(res.has_value());
    CHECK(res.error().code() == StatusCode::NotFound);
}

TEST_CASE("compose UserFlags rejects me==NULL", "[application_chat]") {
    auto r = req_with_me(ac::LegacyMessageType::UserFlags);
    r.me_present = false;
    auto res = ac::compose_chat_event(r);
    REQUIRE_FALSE(res.has_value());
    CHECK(res.error().code() == StatusCode::NotFound);
}

TEST_CASE("compose WhisperAck rejects me==NULL", "[application_chat]") {
    auto r = req_with_me(ac::LegacyMessageType::WhisperAck);
    r.me_present = false;
    auto res = ac::compose_chat_event(r);
    REQUIRE_FALSE(res.has_value());
    CHECK(res.error().code() == StatusCode::NotFound);
}

TEST_CASE("compose FriendWhisperAck rejects me==NULL", "[application_chat]") {
    auto r = req_with_me(ac::LegacyMessageType::FriendWhisperAck);
    r.me_present = false;
    auto res = ac::compose_chat_event(r);
    REQUIRE_FALSE(res.has_value());
    CHECK(res.error().code() == StatusCode::NotFound);
}

TEST_CASE("compose Channel rejects me==NULL", "[application_chat]") {
    auto r = req_with_me(ac::LegacyMessageType::Channel);
    r.me_present = false;
    auto res = ac::compose_chat_event(r);
    REQUIRE_FALSE(res.has_value());
    CHECK(res.error().code() == StatusCode::NotFound);
}

TEST_CASE("compose ChannelDoesNotExist rejects me==NULL", "[application_chat]") {
    auto r = req_with_me(ac::LegacyMessageType::ChannelDoesNotExist);
    r.me_present = false;
    auto res = ac::compose_chat_event(r);
    REQUIRE_FALSE(res.has_value());
    CHECK(res.error().code() == StatusCode::NotFound);
}

TEST_CASE("compose Emote rejects me==NULL", "[application_chat]") {
    auto r = req_with_me(ac::LegacyMessageType::Emote);
    r.me_present = false;
    auto res = ac::compose_chat_event(r);
    REQUIRE_FALSE(res.has_value());
    CHECK(res.error().code() == StatusCode::NotFound);
}
