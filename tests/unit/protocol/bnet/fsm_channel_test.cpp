// SPDX-License-Identifier: GPL-2.0-or-later
//
// BnetFsm Channel Operation Tests
//
// Covers:
//   - SID_CHANNELLIST (0x0B): client sends channel list request, server
//     responds with ChannelListReply containing channel names
//   - SID_JOINCHANNEL (0x0C): client joins a channel, server sends
//     EID_CHANNEL (event_id=7) reply
//   - SID_CHATCOMMAND (0x0E): client sends a chat message, server echoes
//     EID_TALK (event_id=5)
//   - SID_CHATCOMMAND with /help: server responds with EID_INFO (event_id=0x12)
//   - EID_SHOWUSER roster: joining a channel sends EID_CHANNEL as last event
//   - LEAVECHANNEL after JOIN: state stays InChat
//   - Multiple JOINs: each sends EID_CHANNEL
//   - CHATCOMMAND before JOIN: still accepted (InChat state)

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "protocol/bnet/fsm.hpp"
#include "protocol/bnet/messages.hpp"

using namespace pvpgn;
using namespace pvpgn::protocol::bnet;

// ===========================================================================
// Test helpers
// ===========================================================================

namespace {

class FakeBnetCtx : public ISessionContext {
public:
    std::vector<ServerMessage> sent;
    bool closed = false;

    core::Status<> send(const ServerMessage& m) override {
        sent.push_back(m);
        return core::ok();
    }
    void close() override { closed = true; }
};

BnetUseCaseContext make_null_ctx() {
    return BnetUseCaseContext{
        .login_user        = nullptr,
        .create_account    = nullptr,
        .change_password   = nullptr,
        .join_channel      = nullptr,
        .post_message      = nullptr,
        .leave_channel     = nullptr,
        .list_channels     = nullptr,
        .start_game        = nullptr,
        .join_game         = nullptr,
        .leave_game        = nullptr,
        .check_ip_ban      = nullptr,
        .account_repo      = nullptr,
        .command_registry  = nullptr,
        .message_router    = nullptr,
        .permission_checker = nullptr,
        .session_registry  = nullptr,
        .login_user_w3     = nullptr,
        .srp3_store        = nullptr,
        .add_friend        = nullptr,
        .remove_friend     = nullptr,
        .list_friends      = nullptr,
    };
}

/// Drive FSM to InChat state (no use-cases).
void reach_in_chat(BnetFsm& f, std::string_view username = "alice") {
    REQUIRE(f.handle(ClientMessage{AuthInfo{}}).has_value());
    LogonResponse2 logon{};
    logon.username = std::string{username};
    REQUIRE(f.handle(ClientMessage{logon}).has_value());
    EnterChatRequest req{std::string{username}, "PXES"};
    REQUIRE(f.handle(ClientMessage{req}).has_value());
    REQUIRE(f.state() == BnetState::InChat);
}

/// Drive FSM to InChat and join a channel.
void reach_in_channel(BnetFsm& f, std::string_view channel = "Ladder",
                      std::string_view username = "alice") {
    reach_in_chat(f, username);
    REQUIRE(f.handle(ClientMessage{JoinChannel{0, std::string{channel}}}).has_value());
}

/// Count sent messages of a given type.
template <typename T>
std::size_t count_of(const std::vector<ServerMessage>& sent) {
    std::size_t n = 0;
    for (const auto& m : sent)
        if (std::holds_alternative<T>(m)) ++n;
    return n;
}

/// Find the last ChatEvent with the given event_id.
const ChatEvent* last_chat_event_with_id(const std::vector<ServerMessage>& sent,
                                          std::uint32_t event_id) {
    const ChatEvent* result = nullptr;
    for (const auto& m : sent) {
        if (std::holds_alternative<ChatEvent>(m)) {
            const auto& ev = std::get<ChatEvent>(m);
            if (ev.event_id == event_id) result = &ev;
        }
    }
    return result;
}

}  // namespace

// ===========================================================================
// SID_CHANNELLIST (0x0B) — channel list request/response
// ===========================================================================

TEST_CASE("BnetFsm R306: ChannelListRequest in InChat returns ChannelListReply",
          "[protocol][bnet][fsm][channel][R306]") {
    auto ctx = std::make_shared<FakeBnetCtx>();
    auto uc  = make_null_ctx();
    BnetFsm f{ctx, uc};
    reach_in_chat(f);
    ctx->sent.clear();

    ChannelListRequest req{};
    req.client_tag = 0x53544152u;  // 'STAR'
    REQUIRE(f.handle(ClientMessage{req}).has_value());

    // Must have sent at least one ChannelListReply
    REQUIRE(count_of<ChannelListReply>(ctx->sent) >= 1u);
}

TEST_CASE("BnetFsm R306: ChannelListReply contains a channels vector",
          "[protocol][bnet][fsm][channel][R306]") {
    auto ctx = std::make_shared<FakeBnetCtx>();
    auto uc  = make_null_ctx();
    BnetFsm f{ctx, uc};
    reach_in_chat(f);
    ctx->sent.clear();

    ChannelListRequest req{};
    REQUIRE(f.handle(ClientMessage{req}).has_value());

    // Find the ChannelListReply
    bool found = false;
    for (const auto& m : ctx->sent) {
        if (std::holds_alternative<ChannelListReply>(m)) {
            found = true;
            // channels vector must exist (may be empty in stub mode)
            const auto& reply = std::get<ChannelListReply>(m);
            (void)reply.channels;  // just verify it compiles and is accessible
        }
    }
    REQUIRE(found);
}

TEST_CASE("BnetFsm R306: ChannelListRequest before InChat is accepted as no-op",
          "[protocol][bnet][fsm][channel][R306]") {
    auto ctx = std::make_shared<FakeBnetCtx>();
    auto uc  = make_null_ctx();
    BnetFsm f{ctx, uc};

    // In Init state — ChannelListRequest is a no-op (not a state-changing message)
    REQUIRE(f.handle(ClientMessage{AuthInfo{}}).has_value());
    LogonResponse2 logon{};
    logon.username = "alice";
    REQUIRE(f.handle(ClientMessage{logon}).has_value());
    REQUIRE(f.state() == BnetState::LoggedIn);

    // ChannelListRequest in LoggedIn state should be accepted
    ChannelListRequest req{};
    REQUIRE(f.handle(ClientMessage{req}).has_value());
}

// ===========================================================================
// SID_JOINCHANNEL (0x0C) — join channel, EID_CHANNEL reply
// ===========================================================================

TEST_CASE("BnetFsm R306: JOINCHANNEL sends EID_CHANNEL (event_id=7)",
          "[protocol][bnet][fsm][channel][R306]") {
    auto ctx = std::make_shared<FakeBnetCtx>();
    auto uc  = make_null_ctx();
    BnetFsm f{ctx, uc};
    reach_in_chat(f);
    ctx->sent.clear();

    REQUIRE(f.handle(ClientMessage{JoinChannel{0, "Diablo USA-1"}}).has_value());

    // Last message must be EID_CHANNEL (event_id == 7)
    REQUIRE(!ctx->sent.empty());
    REQUIRE(std::holds_alternative<ChatEvent>(ctx->sent.back()));
    const auto& ev = std::get<ChatEvent>(ctx->sent.back());
    REQUIRE(ev.event_id == 7u);  // EID_CHANNEL
    REQUIRE(ev.text == "Diablo USA-1");
}

TEST_CASE("BnetFsm R306: JOINCHANNEL channel name is preserved in EID_CHANNEL",
          "[protocol][bnet][fsm][channel][R306]") {
    auto ctx = std::make_shared<FakeBnetCtx>();
    auto uc  = make_null_ctx();
    BnetFsm f{ctx, uc};
    reach_in_chat(f);
    ctx->sent.clear();

    const std::string channel_name = "Op Allstars";
    REQUIRE(f.handle(ClientMessage{JoinChannel{0, channel_name}}).has_value());

    const auto* ev = last_chat_event_with_id(ctx->sent, 7u);  // EID_CHANNEL
    REQUIRE(ev != nullptr);
    REQUIRE(ev->text == channel_name);
}

TEST_CASE("BnetFsm R306: JOINCHANNEL with flags=1 (force-join) still sends EID_CHANNEL",
          "[protocol][bnet][fsm][channel][R306]") {
    auto ctx = std::make_shared<FakeBnetCtx>();
    auto uc  = make_null_ctx();
    BnetFsm f{ctx, uc};
    reach_in_chat(f);
    ctx->sent.clear();

    // flags=1 means force-join (create if not exists)
    REQUIRE(f.handle(ClientMessage{JoinChannel{1, "Ladder"}}).has_value());

    REQUIRE(count_of<ChatEvent>(ctx->sent) >= 1u);
    const auto* ev = last_chat_event_with_id(ctx->sent, 7u);  // EID_CHANNEL
    REQUIRE(ev != nullptr);
    REQUIRE(ev->text == "Ladder");
}

TEST_CASE("BnetFsm R306: second JOINCHANNEL replaces first channel",
          "[protocol][bnet][fsm][channel][R306]") {
    auto ctx = std::make_shared<FakeBnetCtx>();
    auto uc  = make_null_ctx();
    BnetFsm f{ctx, uc};
    reach_in_chat(f);

    REQUIRE(f.handle(ClientMessage{JoinChannel{0, "Ladder"}}).has_value());
    ctx->sent.clear();

    REQUIRE(f.handle(ClientMessage{JoinChannel{0, "Diablo USA-1"}}).has_value());

    const auto* ev = last_chat_event_with_id(ctx->sent, 7u);  // EID_CHANNEL
    REQUIRE(ev != nullptr);
    REQUIRE(ev->text == "Diablo USA-1");
}

// ===========================================================================
// SID_CHATCOMMAND (0x0E) — chat message echoes EID_TALK
// ===========================================================================

TEST_CASE("BnetFsm R306: CHATCOMMAND echoes EID_TALK (event_id=5)",
          "[protocol][bnet][fsm][channel][R306]") {
    auto ctx = std::make_shared<FakeBnetCtx>();
    auto uc  = make_null_ctx();
    BnetFsm f{ctx, uc};
    reach_in_channel(f);
    ctx->sent.clear();

    REQUIRE(f.handle(ClientMessage{ChatCommand{"hello world"}}).has_value());

    REQUIRE(!ctx->sent.empty());
    REQUIRE(std::holds_alternative<ChatEvent>(ctx->sent.back()));
    const auto& ev = std::get<ChatEvent>(ctx->sent.back());
    REQUIRE(ev.event_id == 5u);  // EID_TALK
    REQUIRE(ev.text == "hello world");
}

TEST_CASE("BnetFsm R306: CHATCOMMAND text is preserved verbatim",
          "[protocol][bnet][fsm][channel][R306]") {
    auto ctx = std::make_shared<FakeBnetCtx>();
    auto uc  = make_null_ctx();
    BnetFsm f{ctx, uc};
    reach_in_channel(f);
    ctx->sent.clear();

    const std::string message = "GG WP everyone!";
    REQUIRE(f.handle(ClientMessage{ChatCommand{message}}).has_value());

    const auto* ev = last_chat_event_with_id(ctx->sent, 5u);
    REQUIRE(ev != nullptr);
    REQUIRE(ev->text == message);
}

TEST_CASE("BnetFsm R306: CHATCOMMAND empty message is accepted",
          "[protocol][bnet][fsm][channel][R306]") {
    auto ctx = std::make_shared<FakeBnetCtx>();
    auto uc  = make_null_ctx();
    BnetFsm f{ctx, uc};
    reach_in_channel(f);
    ctx->sent.clear();

    REQUIRE(f.handle(ClientMessage{ChatCommand{""}}).has_value());
    // No crash, no close
    REQUIRE(!ctx->closed);
}

// ===========================================================================
// SID_CHATCOMMAND with /help — server responds with EID_INFO
// ===========================================================================

TEST_CASE("BnetFsm R306: CHATCOMMAND /help returns EID_INFO (event_id=0x12)",
          "[protocol][bnet][fsm][channel][R306]") {
    auto ctx = std::make_shared<FakeBnetCtx>();
    auto uc  = make_null_ctx();
    BnetFsm f{ctx, uc};
    reach_in_channel(f);
    ctx->sent.clear();

    REQUIRE(f.handle(ClientMessage{ChatCommand{"/help"}}).has_value());

    REQUIRE(!ctx->sent.empty());
    REQUIRE(std::holds_alternative<ChatEvent>(ctx->sent.back()));
    const auto& ev = std::get<ChatEvent>(ctx->sent.back());
    REQUIRE(ev.event_id == 0x12u);  // EID_INFO
}

TEST_CASE("BnetFsm R306: CHATCOMMAND /who returns EID_INFO (event_id=0x12)",
          "[protocol][bnet][fsm][channel][R306]") {
    auto ctx = std::make_shared<FakeBnetCtx>();
    auto uc  = make_null_ctx();
    BnetFsm f{ctx, uc};
    reach_in_channel(f);
    ctx->sent.clear();

    REQUIRE(f.handle(ClientMessage{ChatCommand{"/who"}}).has_value());

    // Any slash-command returns EID_INFO
    const auto* ev = last_chat_event_with_id(ctx->sent, 0x12u);  // EID_INFO
    REQUIRE(ev != nullptr);
}

TEST_CASE("BnetFsm R306: CHATCOMMAND /unknown_cmd returns EID_INFO (event_id=0x12)",
          "[protocol][bnet][fsm][channel][R306]") {
    auto ctx = std::make_shared<FakeBnetCtx>();
    auto uc  = make_null_ctx();
    BnetFsm f{ctx, uc};
    reach_in_channel(f);
    ctx->sent.clear();

    REQUIRE(f.handle(ClientMessage{ChatCommand{"/xyzzy_unknown"}}).has_value());

    const auto* ev = last_chat_event_with_id(ctx->sent, 0x12u);  // EID_INFO
    REQUIRE(ev != nullptr);
}

// ===========================================================================
// EID_SHOWUSER roster — joining a channel with existing members
// ===========================================================================

TEST_CASE("BnetFsm R306: JOINCHANNEL sends at least one ChatEvent",
          "[protocol][bnet][fsm][channel][R306]") {
    auto ctx = std::make_shared<FakeBnetCtx>();
    auto uc  = make_null_ctx();
    BnetFsm f{ctx, uc};
    reach_in_chat(f);
    ctx->sent.clear();

    REQUIRE(f.handle(ClientMessage{JoinChannel{0, "Ladder"}}).has_value());

    // At minimum, EID_CHANNEL (event_id=7) must be sent
    REQUIRE(count_of<ChatEvent>(ctx->sent) >= 1u);
    const auto* channel_ev = last_chat_event_with_id(ctx->sent, 7u);
    REQUIRE(channel_ev != nullptr);
}

TEST_CASE("BnetFsm R306: JOINCHANNEL EID_CHANNEL is the last ChatEvent sent",
          "[protocol][bnet][fsm][channel][R306]") {
    auto ctx = std::make_shared<FakeBnetCtx>();
    auto uc  = make_null_ctx();
    BnetFsm f{ctx, uc};
    reach_in_chat(f);
    ctx->sent.clear();

    REQUIRE(f.handle(ClientMessage{JoinChannel{0, "Ladder"}}).has_value());

    // The last ChatEvent must be EID_CHANNEL (event_id=7)
    // (EID_SHOWUSER events for existing members come before EID_CHANNEL)
    REQUIRE(std::holds_alternative<ChatEvent>(ctx->sent.back()));
    REQUIRE(std::get<ChatEvent>(ctx->sent.back()).event_id == 7u);
}

// ===========================================================================
// LEAVECHANNEL after JOIN
// ===========================================================================

TEST_CASE("BnetFsm R306: LEAVECHANNEL after JOIN keeps InChat state",
          "[protocol][bnet][fsm][channel][R306]") {
    auto ctx = std::make_shared<FakeBnetCtx>();
    auto uc  = make_null_ctx();
    BnetFsm f{ctx, uc};
    reach_in_channel(f);
    ctx->sent.clear();

    REQUIRE(f.handle(ClientMessage{LeaveChannel{}}).has_value());
    REQUIRE(f.state() == BnetState::InChat);
    REQUIRE(!ctx->closed);
}

TEST_CASE("BnetFsm R306: CHATCOMMAND in InChat without prior JOIN is accepted",
          "[protocol][bnet][fsm][channel][R306]") {
    auto ctx = std::make_shared<FakeBnetCtx>();
    auto uc  = make_null_ctx();
    BnetFsm f{ctx, uc};
    reach_in_chat(f);
    ctx->sent.clear();

    // ChatCommand in InChat state (no channel joined yet) should not crash
    REQUIRE(f.handle(ClientMessage{ChatCommand{"hello"}}).has_value());
    REQUIRE(!ctx->closed);
}
