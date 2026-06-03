// SPDX-License-Identifier: GPL-2.0-or-later
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

constexpr std::uint32_t kMagic = 0xBAADF00Du;

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

TEST_CASE("compose Talk: full fields", "[application_chat]") {
    auto r = req_with_me(ac::LegacyMessageType::Talk);
    auto res = ac::compose_chat_event(r);
    REQUIRE(res.has_value());
    auto const& ev = res.value();
    CHECK(ev.event_id     == pb::kServerMessageTypeTalk);
    CHECK(ev.flags        == (0x12u | 0x80u));
    CHECK(ev.ping_ms      == 50u);
    CHECK(ev.user_ip      == 0u);
    CHECK(ev.acct_number  == kMagic);
    CHECK(ev.registration == kMagic);
    CHECK(ev.username     == "Bob");
    CHECK(ev.text         == "hi");
}

TEST_CASE("compose Talk rejects MF_X (ignored)", "[application_chat]") {
    auto r = req_with_me(ac::LegacyMessageType::Talk);
    r.dstflags_mf_x = true;
    auto res = ac::compose_chat_event(r);
    REQUIRE_FALSE(res.has_value());
    CHECK(res.error().code() == StatusCode::NotFound);
}

TEST_CASE("compose Talk rejects me==NULL", "[application_chat]") {
    auto r = req_with_me(ac::LegacyMessageType::Talk);
    r.me_present = false;
    auto res = ac::compose_chat_event(r);
    REQUIRE_FALSE(res.has_value());
    CHECK(res.error().code() == StatusCode::NotFound);
}

TEST_CASE("compose Info: zero flags/latency, empty username", "[application_chat]") {
    auto r = req_with_me(ac::LegacyMessageType::Info);
    auto res = ac::compose_chat_event(r);
    REQUIRE(res.has_value());
    auto const& ev = res.value();
    CHECK(ev.event_id == pb::kServerMessageTypeInfo);
    CHECK(ev.flags    == 0u);
    CHECK(ev.ping_ms  == 0u);
    CHECK(ev.username == "");
    CHECK(ev.text     == "hi");
}

TEST_CASE("compose Error: zero flags/latency, empty username", "[application_chat]") {
    auto r = req_with_me(ac::LegacyMessageType::Error);
    auto res = ac::compose_chat_event(r);
    REQUIRE(res.has_value());
    auto const& ev = res.value();
    CHECK(ev.event_id == pb::kServerMessageTypeError);
    CHECK(ev.flags    == 0u);
    CHECK(ev.ping_ms  == 0u);
    CHECK(ev.username == "");
    CHECK(ev.text     == "hi");
}

TEST_CASE("compose ChannelFull: all-empty payload", "[application_chat]") {
    auto r = req_with_me(ac::LegacyMessageType::ChannelFull);
    auto res = ac::compose_chat_event(r);
    REQUIRE(res.has_value());
    auto const& ev = res.value();
    CHECK(ev.event_id == pb::kServerMessageTypeChannelFull);
    CHECK(ev.flags    == 0u);
    CHECK(ev.ping_ms  == 0u);
    CHECK(ev.username == "");
    CHECK(ev.text     == "");
}

TEST_CASE("compose ChannelRestricted: all-empty payload", "[application_chat]") {
    auto r = req_with_me(ac::LegacyMessageType::ChannelRestricted);
    auto res = ac::compose_chat_event(r);
    REQUIRE(res.has_value());
    auto const& ev = res.value();
    CHECK(ev.event_id == pb::kServerMessageTypeChannelRestricted);
    CHECK(ev.flags    == 0u);
    CHECK(ev.ping_ms  == 0u);
    CHECK(ev.username == "");
    CHECK(ev.text     == "");
}

TEST_CASE("compose Emote: combined flags + chatcharname", "[application_chat]") {
    auto r = req_with_me(ac::LegacyMessageType::Emote);
    auto res = ac::compose_chat_event(r);
    REQUIRE(res.has_value());
    auto const& ev = res.value();
    CHECK(ev.event_id == pb::kServerMessageTypeEmote);
    CHECK(ev.flags    == (0x12u | 0x80u));
    CHECK(ev.ping_ms  == 50u);
    CHECK(ev.username == "Bob");
    CHECK(ev.text     == "hi");
}

TEST_CASE("compose Whisper: me==NULL falls back to servername", "[application_chat]") {
    auto r = req_with_me(ac::LegacyMessageType::Whisper);
    r.me_present = false;
    r.servername = "PvPGN";
    auto res = ac::compose_chat_event(r);
    REQUIRE(res.has_value());
    auto const& ev = res.value();
    CHECK(ev.event_id == pb::kServerMessageTypeWhisper);
    CHECK(ev.flags    == 0x80u);
    CHECK(ev.ping_ms  == 0u);
    CHECK(ev.username == "PvPGN");
    CHECK(ev.text     == "hi");
}

TEST_CASE("compose Whisper rejects MF_X", "[application_chat]") {
    auto r = req_with_me(ac::LegacyMessageType::Whisper);
    r.dstflags_mf_x = true;
    auto res = ac::compose_chat_event(r);
    REQUIRE_FALSE(res.has_value());
    CHECK(res.error().code() == StatusCode::NotFound);
}

TEST_CASE("compose Join rejects me==dst", "[application_chat]") {
    auto r = req_with_me(ac::LegacyMessageType::Join);
    r.dst_eq_me = true;
    auto res = ac::compose_chat_event(r);
    REQUIRE_FALSE(res.has_value());
    CHECK(res.error().code() == StatusCode::NotFound);
}

TEST_CASE("compose AddUser: playerinfo carried in text", "[application_chat]") {
    auto r = req_with_me(ac::LegacyMessageType::AddUser);
    auto res = ac::compose_chat_event(r);
    REQUIRE(res.has_value());
    auto const& ev = res.value();
    CHECK(ev.event_id == pb::kServerMessageTypeAddUser);
    CHECK(ev.flags    == (0x12u | 0x80u));
    CHECK(ev.username == "Bob");
    CHECK(ev.text     == "1RAW");
}

TEST_CASE("compose Channel: uses channel_flags_bncflags and chatname", "[application_chat]") {
    auto r = req_with_me(ac::LegacyMessageType::Channel);
    r.channel_flags_bncflags = 0x00000010u;
    r.chatname               = "Chat";
    auto res = ac::compose_chat_event(r);
    REQUIRE(res.has_value());
    auto const& ev = res.value();
    CHECK(ev.event_id == pb::kServerMessageTypeChannel);
    CHECK(ev.flags    == 0x00000010u);
    CHECK(ev.username == "Chat");
    CHECK(ev.text     == "hi");
}

TEST_CASE("compose FriendWhisperAck: username forced to 'your friends'", "[application_chat]") {
    auto r = req_with_me(ac::LegacyMessageType::FriendWhisperAck);
    auto res = ac::compose_chat_event(r);
    REQUIRE(res.has_value());
    auto const& ev = res.value();
    CHECK(ev.event_id == pb::kServerMessageTypeWhisperAck);
    CHECK(ev.username == "your friends");
    CHECK(ev.text     == "hi");
}

// --- previously-uncovered LegacyMessageType arms ---------------------------

TEST_CASE("compose Part: username only, empty text", "[application_chat]") {
    auto res = ac::compose_chat_event(req_with_me(ac::LegacyMessageType::Part));
    REQUIRE(res.has_value());
    CHECK(res.value().event_id == pb::kServerMessageTypePart);
    CHECK(res.value().username == "Bob");
    CHECK(res.value().text == "");
}

TEST_CASE("compose Part rejects me==NULL", "[application_chat]") {
    auto r = req_with_me(ac::LegacyMessageType::Part);
    r.me_present = false;
    auto res = ac::compose_chat_event(r);
    REQUIRE_FALSE(res.has_value());
    CHECK(res.error().code() == StatusCode::NotFound);
}

TEST_CASE("compose Broadcast: full fields", "[application_chat]") {
    auto res = ac::compose_chat_event(req_with_me(ac::LegacyMessageType::Broadcast));
    REQUIRE(res.has_value());
    CHECK(res.value().event_id == pb::kServerMessageTypeBroadcast);
    CHECK(res.value().username == "Bob");
    CHECK(res.value().text == "hi");
}

TEST_CASE("compose Broadcast rejects MF_X", "[application_chat]") {
    auto r = req_with_me(ac::LegacyMessageType::Broadcast);
    r.dstflags_mf_x = true;
    auto res = ac::compose_chat_event(r);
    REQUIRE_FALSE(res.has_value());
    CHECK(res.error().code() == StatusCode::NotFound);
}

TEST_CASE("compose UserFlags: text from playerinfo", "[application_chat]") {
    auto res = ac::compose_chat_event(req_with_me(ac::LegacyMessageType::UserFlags));
    REQUIRE(res.has_value());
    CHECK(res.value().event_id == pb::kServerMessageTypeUserFlags);
    CHECK(res.value().username == "Bob");
    CHECK(res.value().text == "1RAW");
}

TEST_CASE("compose WhisperAck: full fields", "[application_chat]") {
    auto res = ac::compose_chat_event(req_with_me(ac::LegacyMessageType::WhisperAck));
    REQUIRE(res.has_value());
    CHECK(res.value().event_id == pb::kServerMessageTypeWhisperAck);
    CHECK(res.value().username == "Bob");
    CHECK(res.value().text == "hi");
}

TEST_CASE("compose ChannelDoesNotExist: username from chatname",
          "[application_chat]") {
    auto res =
        ac::compose_chat_event(req_with_me(ac::LegacyMessageType::ChannelDoesNotExist));
    REQUIRE(res.has_value());
    CHECK(res.value().event_id == pb::kServerMessageTypeChannelDoesNotExist);
    CHECK(res.value().username == "Bob");
    CHECK(res.value().text == "hi");
}
