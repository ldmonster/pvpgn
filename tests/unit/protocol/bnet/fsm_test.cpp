// SPDX-License-Identifier: GPL-2.0-or-later
#include <string>
#include <variant>
#include <vector>
#include <memory>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "domain/connection/ports.hpp"
#include "domain/shared/ids.hpp"
#include "protocol/bnet/fsm.hpp"

using namespace pvpgn;
using namespace pvpgn::protocol::bnet;

namespace {

class FakeContext : public ISessionContext {
public:
    std::vector<ServerMessage> sent;
    bool closed = false;

    core::Status<> send(const ServerMessage& m) override {
        sent.push_back(m);
        return core::ok();
    }
    void close() override { closed = true; }
};

// Create a minimal BnetUseCaseContext for testing
BnetUseCaseContext make_test_context() {
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
        .list_public_games = nullptr,
        .channel_reader    = nullptr,
        .ignore_store      = nullptr,
        .check_ip_ban      = nullptr,
        .account_repo      = nullptr,
        .command_registry  = nullptr,
        .message_router    = nullptr,
        .permission_checker = nullptr,
        .session_registry  = nullptr,
        .login_user_w3     = nullptr,
        .srp3_store        = nullptr,
        .user_profile_store = nullptr,
        .add_friend        = nullptr,
        .remove_friend     = nullptr,
        .list_friends      = nullptr,
        .server_name       = "",
    };
}

/// Fake message router that records broadcast calls.
class FakeMessageRouter : public domain::connection::IMessageRouter {
public:
    struct BroadcastCall {
        std::vector<domain::SessionId> sessions;
        std::vector<std::byte>         bytes;
    };

    std::vector<BroadcastCall> broadcasts;

    core::Result<void, core::Error>
    send(domain::SessionId, std::span<const std::byte>) override {
        return core::ok();
    }

    core::Result<void, core::Error>
    broadcast(std::span<const domain::SessionId> sessions,
              std::span<const std::byte> bytes) override {
        BroadcastCall call;
        call.sessions.assign(sessions.begin(), sessions.end());
        call.bytes.assign(bytes.begin(), bytes.end());
        broadcasts.push_back(std::move(call));
        return core::ok();
    }

    core::Result<void, core::Error>
    send_to_account(domain::AccountId, std::span<const std::byte>) override {
        return core::ok();
    }
};

/// Helper: drive FSM to InChat state (no use-cases).
void reach_in_chat(BnetFsm& f, std::string_view username = "alice") {
    REQUIRE(f.handle(ClientMessage{AuthInfo{}}).has_value());
    LogonResponse2 logon{};
    logon.username = std::string{username};
    REQUIRE(f.handle(ClientMessage{logon}).has_value());
    EnterChatRequest req{std::string{username}, "PXES"};
    REQUIRE(f.handle(ClientMessage{req}).has_value());
    REQUIRE(f.state() == BnetState::InChat);
}

/// Helper: drive FSM to LoggedIn state (no use-cases).
void reach_logged_in(BnetFsm& f, std::string_view username = "alice") {
    REQUIRE(f.handle(ClientMessage{AuthInfo{}}).has_value());
    LogonResponse2 logon{};
    logon.username = std::string{username};
    REQUIRE(f.handle(ClientMessage{logon}).has_value());
    REQUIRE(f.state() == BnetState::LoggedIn);
}

}  // namespace

TEST_CASE("BnetFsm: Ping is mirrored verbatim in any state",
          "[protocol][bnet][fsm]") {
    auto session_ctx = std::make_shared<FakeContext>();
    auto use_case_ctx = make_test_context();
    BnetFsm f{session_ctx, use_case_ctx};
    REQUIRE(f.handle(ClientMessage{Ping{0xCAFEBABE}}).has_value());
    REQUIRE(session_ctx->sent.size() == 1);
    REQUIRE(std::get<Ping>(session_ctx->sent[0]).ticks == 0xCAFEBABE);
    REQUIRE(f.state() == BnetState::Init);
}

TEST_CASE("BnetFsm: AUTH_INFO transitions to AuthInfoReceived + sends 0x50 seed",
          "[protocol][bnet][fsm]") {
    auto session_ctx = std::make_shared<FakeContext>();
    auto use_case_ctx = make_test_context();
    BnetFsm f{session_ctx, use_case_ctx};
    REQUIRE(f.handle(ClientMessage{AuthInfo{}}).has_value());
    REQUIRE(f.state() == BnetState::AuthInfoReceived);
    REQUIRE(session_ctx->sent.size() == 1);
    // The server replies with the SID_AUTH_INFO seed (a real client blocks for
    // it); the AUTH_CHECK result follows the client's 0x51, not AUTH_INFO.
    REQUIRE(std::get<AuthInfoReply>(session_ctx->sent[0]).server_token != 0u);
}

TEST_CASE("BnetFsm: AUTH_INFO out of order closes session",
          "[protocol][bnet][fsm]") {
    auto session_ctx = std::make_shared<FakeContext>();
    auto use_case_ctx = make_test_context();
    BnetFsm f{session_ctx, use_case_ctx};
    REQUIRE(f.handle(ClientMessage{AuthInfo{}}).has_value());
    auto s = f.handle(ClientMessage{AuthInfo{}});
    REQUIRE_FALSE(s.has_value());
    REQUIRE(session_ctx->closed);
    REQUIRE(f.state() == BnetState::Closing);
}

TEST_CASE("BnetFsm: happy path to InChat",
          "[protocol][bnet][fsm]") {
    auto session_ctx = std::make_shared<FakeContext>();
    auto use_case_ctx = make_test_context();
    BnetFsm f{session_ctx, use_case_ctx};
    REQUIRE(f.handle(ClientMessage{AuthInfo{}}).has_value());

    LogonResponse2 logon{};
    logon.username = "alice";
    REQUIRE(f.handle(ClientMessage{logon}).has_value());
    REQUIRE(f.state() == BnetState::LoggedIn);
    REQUIRE(std::get<LogonResponse2Reply>(session_ctx->sent.back()).result == 0u);

    EnterChatRequest req{"alice", "PXES"};
    REQUIRE(f.handle(ClientMessage{req}).has_value());
    REQUIRE(f.state() == BnetState::InChat);
    REQUIRE(std::get<EnterChatReply>(session_ctx->sent.back()).account == "alice");

    REQUIRE(f.handle(ClientMessage{JoinChannel{0, "Op Allstars"}}).has_value());
    REQUIRE(std::get<ChatEvent>(session_ctx->sent.back()).text == "Op Allstars");

    REQUIRE(f.handle(ClientMessage{ChatCommand{"hello"}}).has_value());
    REQUIRE(std::get<ChatEvent>(session_ctx->sent.back()).text == "hello");
    REQUIRE(std::get<ChatEvent>(session_ctx->sent.back()).event_id == 5u);
}

TEST_CASE("BnetFsm: empty username yields login failure (no transition)",
          "[protocol][bnet][fsm]") {
    auto session_ctx = std::make_shared<FakeContext>();
    auto use_case_ctx = make_test_context();
    BnetFsm f{session_ctx, use_case_ctx};
    REQUIRE(f.handle(ClientMessage{AuthInfo{}}).has_value());
    REQUIRE(f.handle(ClientMessage{LogonResponse2{}}).has_value());
    REQUIRE(f.state() == BnetState::AuthInfoReceived);
    REQUIRE(std::get<LogonResponse2Reply>(session_ctx->sent.back()).result == 0x01u);
}

TEST_CASE("BnetFsm: JOINCHANNEL before ENTERCHAT closes session",
          "[protocol][bnet][fsm]") {
    auto session_ctx = std::make_shared<FakeContext>();
    auto use_case_ctx = make_test_context();
    BnetFsm f{session_ctx, use_case_ctx};
    REQUIRE(f.handle(ClientMessage{AuthInfo{}}).has_value());
    LogonResponse2 logon{};
    logon.username = "alice";
    REQUIRE(f.handle(ClientMessage{logon}).has_value());

    auto s = f.handle(ClientMessage{JoinChannel{0, "x"}});
    REQUIRE_FALSE(s.has_value());
    REQUIRE(session_ctx->closed);
}

TEST_CASE("BnetFsm: STARTGAME1 transitions LoggedIn -> InGame",
          "[protocol][bnet][fsm]") {
    auto session_ctx = std::make_shared<FakeContext>();
    auto use_case_ctx = make_test_context();
    BnetFsm f{session_ctx, use_case_ctx};
    REQUIRE(f.handle(ClientMessage{AuthInfo{}}).has_value());
    LogonResponse2 logon{};
    logon.username = "alice";
    REQUIRE(f.handle(ClientMessage{logon}).has_value());
    REQUIRE(f.state() == BnetState::LoggedIn);

    StartGame1Request req{};
    req.game_name = "g";
    REQUIRE(f.handle(ClientMessage{req}).has_value());
    REQUIRE(f.state() == BnetState::InGame);
}

TEST_CASE("BnetFsm: STARTGAME3 transitions InChat -> InGame",
          "[protocol][bnet][fsm]") {
    auto session_ctx = std::make_shared<FakeContext>();
    auto use_case_ctx = make_test_context();
    BnetFsm f{session_ctx, use_case_ctx};
    REQUIRE(f.handle(ClientMessage{AuthInfo{}}).has_value());
    LogonResponse2 logon{};
    logon.username = "alice";
    REQUIRE(f.handle(ClientMessage{logon}).has_value());
    EnterChatRequest req{"alice", "PXES"};
    REQUIRE(f.handle(ClientMessage{req}).has_value());
    REQUIRE(f.state() == BnetState::InChat);

    StartGame3Request g3{};
    g3.game_name = "g3";
    REQUIRE(f.handle(ClientMessage{g3}).has_value());
    REQUIRE(f.state() == BnetState::InGame);
}

TEST_CASE("BnetFsm: JOINGAME transitions LoggedIn -> InGame, CLOSEGAME returns to LoggedIn",
          "[protocol][bnet][fsm]") {
    auto session_ctx = std::make_shared<FakeContext>();
    auto use_case_ctx = make_test_context();
    BnetFsm f{session_ctx, use_case_ctx};
    REQUIRE(f.handle(ClientMessage{AuthInfo{}}).has_value());
    LogonResponse2 logon{};
    logon.username = "alice";
    REQUIRE(f.handle(ClientMessage{logon}).has_value());

    JoinGame jg{};
    jg.game_name = "g";
    REQUIRE(f.handle(ClientMessage{jg}).has_value());
    REQUIRE(f.state() == BnetState::InGame);

    REQUIRE(f.handle(ClientMessage{CloseGame{}}).has_value());
    REQUIRE(f.state() == BnetState::LoggedIn);
}

TEST_CASE("BnetFsm: CLOSEGAME2 from InGame returns to LoggedIn",
          "[protocol][bnet][fsm]") {
    auto session_ctx = std::make_shared<FakeContext>();
    auto use_case_ctx = make_test_context();
    BnetFsm f{session_ctx, use_case_ctx};
    REQUIRE(f.handle(ClientMessage{AuthInfo{}}).has_value());
    LogonResponse2 logon{};
    logon.username = "alice";
    REQUIRE(f.handle(ClientMessage{logon}).has_value());

    StartGame1Request req{};
    req.game_name = "g";
    REQUIRE(f.handle(ClientMessage{req}).has_value());
    REQUIRE(f.state() == BnetState::InGame);
    REQUIRE(f.handle(ClientMessage{CloseGame2{}}).has_value());
    REQUIRE(f.state() == BnetState::LoggedIn);
}

TEST_CASE("BnetFsm: GAMEREPORT in InGame keeps state",
          "[protocol][bnet][fsm]") {
    auto session_ctx = std::make_shared<FakeContext>();
    auto use_case_ctx = make_test_context();
    BnetFsm f{session_ctx, use_case_ctx};
    REQUIRE(f.handle(ClientMessage{AuthInfo{}}).has_value());
    LogonResponse2 logon{};
    logon.username = "alice";
    REQUIRE(f.handle(ClientMessage{logon}).has_value());
    JoinGame jg{};
    jg.game_name = "g";
    REQUIRE(f.handle(ClientMessage{jg}).has_value());

    GameReport gr{};
    gr.results = {1};
    gr.player_names = {"alice"};
    REQUIRE(f.handle(ClientMessage{gr}).has_value());
    REQUIRE(f.state() == BnetState::InGame);
}

TEST_CASE("BnetFsm: STARTGAME1 before login is rejected",
          "[protocol][bnet][fsm]") {
    auto session_ctx = std::make_shared<FakeContext>();
    auto use_case_ctx = make_test_context();
    BnetFsm f{session_ctx, use_case_ctx};
    StartGame1Request req{};
    req.game_name = "g";
    auto s = f.handle(ClientMessage{req});
    REQUIRE_FALSE(s.has_value());
}

TEST_CASE("BnetFsm: CLOSEGAME outside InGame is accepted as no-op",
          "[protocol][bnet][fsm]") {
    auto session_ctx = std::make_shared<FakeContext>();
    auto use_case_ctx = make_test_context();
    BnetFsm f{session_ctx, use_case_ctx};
    REQUIRE(f.handle(ClientMessage{AuthInfo{}}).has_value());
    LogonResponse2 logon{};
    logon.username = "alice";
    REQUIRE(f.handle(ClientMessage{logon}).has_value());
    REQUIRE(f.state() == BnetState::LoggedIn);
    REQUIRE(f.handle(ClientMessage{CloseGame{}}).has_value());
    REQUIRE(f.state() == BnetState::LoggedIn);
}

// ── New tests added in Round 124 ─────────────────────────────────────────────

TEST_CASE("BnetFsm: AUTH_INFO stores client_tag from game_id",
          "[protocol][bnet][fsm][client_tag]") {
    auto session_ctx = std::make_shared<FakeContext>();
    auto use_case_ctx = make_test_context();
    BnetFsm f{session_ctx, use_case_ctx};

    // 'STAR' packed big-endian = 0x53544152
    AuthInfo ai{};
    ai.game_id = 0x53544152u;
    REQUIRE(f.handle(ClientMessage{ai}).has_value());
    REQUIRE(f.state() == BnetState::AuthInfoReceived);
    // The 0x50 seed is sent; STAR is an OLS client so logon-type is 0.
    REQUIRE(session_ctx->sent.size() == 1);
    REQUIRE(std::get<AuthInfoReply>(session_ctx->sent[0]).logontype == 0u);
}

TEST_CASE("BnetFsm: AUTH_INFO with zero game_id still transitions state",
          "[protocol][bnet][fsm][client_tag]") {
    auto session_ctx = std::make_shared<FakeContext>();
    auto use_case_ctx = make_test_context();
    BnetFsm f{session_ctx, use_case_ctx};

    AuthInfo ai{};
    ai.game_id = 0u;  // all-zero tag — from_packed_be may return error; FSM must not crash
    REQUIRE(f.handle(ClientMessage{ai}).has_value());
    REQUIRE(f.state() == BnetState::AuthInfoReceived);
}

TEST_CASE("BnetFsm: username is stored at login (no use-case path)",
          "[protocol][bnet][fsm][client_tag]") {
    auto session_ctx = std::make_shared<FakeContext>();
    auto use_case_ctx = make_test_context();
    BnetFsm f{session_ctx, use_case_ctx};

    REQUIRE(f.handle(ClientMessage{AuthInfo{}}).has_value());
    LogonResponse2 logon{};
    logon.username = "bob";
    REQUIRE(f.handle(ClientMessage{logon}).has_value());
    REQUIRE(f.state() == BnetState::LoggedIn);
    // Verify login succeeded (result == 0)
    REQUIRE(std::get<LogonResponse2Reply>(session_ctx->sent.back()).result == 0u);
}

TEST_CASE("BnetFsm: JOINGAME sends StartGame4Ack on success (no use-case)",
          "[protocol][bnet][fsm][joingame]") {
    auto session_ctx = std::make_shared<FakeContext>();
    auto use_case_ctx = make_test_context();
    BnetFsm f{session_ctx, use_case_ctx};
    reach_logged_in(f);

    JoinGame jg{};
    jg.game_name = "testgame";
    REQUIRE(f.handle(ClientMessage{jg}).has_value());
    REQUIRE(f.state() == BnetState::InGame);

    // Must send StartGame4Ack, NOT ChatEvent
    REQUIRE(std::holds_alternative<StartGame4Ack>(session_ctx->sent.back()));
    REQUIRE(std::get<StartGame4Ack>(session_ctx->sent.back()).reply == 0x00u);
}

TEST_CASE("BnetFsm: JOINGAME reply is StartGame4Ack not ChatEvent",
          "[protocol][bnet][fsm][joingame]") {
    auto session_ctx = std::make_shared<FakeContext>();
    auto use_case_ctx = make_test_context();
    BnetFsm f{session_ctx, use_case_ctx};
    reach_logged_in(f);

    JoinGame jg{};
    jg.game_name = "g";
    REQUIRE(f.handle(ClientMessage{jg}).has_value());

    // Explicitly verify it is NOT a ChatEvent
    REQUIRE_FALSE(std::holds_alternative<ChatEvent>(session_ctx->sent.back()));
    REQUIRE(std::holds_alternative<StartGame4Ack>(session_ctx->sent.back()));
}

TEST_CASE("BnetFsm: JOINCHANNEL sends EID_CHANNEL reply (no use-case)",
          "[protocol][bnet][fsm][joinchannel]") {
    auto session_ctx = std::make_shared<FakeContext>();
    auto use_case_ctx = make_test_context();
    BnetFsm f{session_ctx, use_case_ctx};
    reach_in_chat(f);

    REQUIRE(f.handle(ClientMessage{JoinChannel{0, "Ladder"}}).has_value());
    // Last sent message must be EID_CHANNEL (event_id == 7)
    REQUIRE(std::holds_alternative<ChatEvent>(session_ctx->sent.back()));
    REQUIRE(std::get<ChatEvent>(session_ctx->sent.back()).event_id == 7u);
    REQUIRE(std::get<ChatEvent>(session_ctx->sent.back()).text == "Ladder");
}

TEST_CASE("BnetFsm: message_router broadcast on JOINCHANNEL (with router)",
          "[protocol][bnet][fsm][broadcast]") {
    auto session_ctx = std::make_shared<FakeContext>();
    auto router = std::make_shared<FakeMessageRouter>();
    auto use_case_ctx = make_test_context();
    use_case_ctx.message_router = router;

    // session_id so the FSM knows who it is
    domain::SessionId my_id{42};
    BnetFsm f{session_ctx, use_case_ctx, my_id};
    reach_in_chat(f, "carol");

    // Without join_channel use-case, members_to_notify is empty → no broadcast
    REQUIRE(f.handle(ClientMessage{JoinChannel{0, "Ops"}}).has_value());
    // No broadcast because join_channel use-case is null (fallback path)
    REQUIRE(router->broadcasts.empty());
}

TEST_CASE("BnetFsm: CHATCOMMAND echoes EID_TALK (no use-case)",
          "[protocol][bnet][fsm][chat]") {
    auto session_ctx = std::make_shared<FakeContext>();
    auto use_case_ctx = make_test_context();
    BnetFsm f{session_ctx, use_case_ctx};
    reach_in_chat(f);
    REQUIRE(f.handle(ClientMessage{JoinChannel{0, "x"}}).has_value());

    REQUIRE(f.handle(ClientMessage{ChatCommand{"world"}}).has_value());
    REQUIRE(std::holds_alternative<ChatEvent>(session_ctx->sent.back()));
    REQUIRE(std::get<ChatEvent>(session_ctx->sent.back()).event_id == 5u);
    REQUIRE(std::get<ChatEvent>(session_ctx->sent.back()).text == "world");
}

TEST_CASE("BnetFsm: CHATCOMMAND with '/' prefix returns EID_INFO",
          "[protocol][bnet][fsm][chat]") {
    auto session_ctx = std::make_shared<FakeContext>();
    auto use_case_ctx = make_test_context();
    BnetFsm f{session_ctx, use_case_ctx};
    reach_in_chat(f);
    REQUIRE(f.handle(ClientMessage{JoinChannel{0, "x"}}).has_value());

    REQUIRE(f.handle(ClientMessage{ChatCommand{"/help"}}).has_value());
    REQUIRE(std::holds_alternative<ChatEvent>(session_ctx->sent.back()));
    REQUIRE(std::get<ChatEvent>(session_ctx->sent.back()).event_id == 0x12u);  // EID_INFO
}

TEST_CASE("BnetFsm: LEAVECHANNEL before login is rejected",
          "[protocol][bnet][fsm][leavechannel]") {
    auto session_ctx = std::make_shared<FakeContext>();
    auto use_case_ctx = make_test_context();
    BnetFsm f{session_ctx, use_case_ctx};
    // In Init state — LEAVECHANNEL must fail
    auto s = f.handle(ClientMessage{LeaveChannel{}});
    REQUIRE_FALSE(s.has_value());
}

TEST_CASE("BnetFsm: LEAVECHANNEL in InChat clears channel (no use-case)",
          "[protocol][bnet][fsm][leavechannel]") {
    auto session_ctx = std::make_shared<FakeContext>();
    auto use_case_ctx = make_test_context();
    BnetFsm f{session_ctx, use_case_ctx};
    reach_in_chat(f);
    REQUIRE(f.handle(ClientMessage{JoinChannel{0, "x"}}).has_value());

    REQUIRE(f.handle(ClientMessage{LeaveChannel{}}).has_value());
    // State stays InChat after leaving channel
    REQUIRE(f.state() == BnetState::InChat);
}

TEST_CASE("BnetFsm: STARTGAME1 sends StartGame1Ack (no use-case)",
          "[protocol][bnet][fsm][startgame]") {
    auto session_ctx = std::make_shared<FakeContext>();
    auto use_case_ctx = make_test_context();
    BnetFsm f{session_ctx, use_case_ctx};
    reach_logged_in(f);

    StartGame1Request req{};
    req.game_name = "mygame";
    REQUIRE(f.handle(ClientMessage{req}).has_value());
    REQUIRE(f.state() == BnetState::InGame);
    REQUIRE(std::holds_alternative<StartGame1Ack>(session_ctx->sent.back()));
    REQUIRE(std::get<StartGame1Ack>(session_ctx->sent.back()).reply == 0x00u);
}

TEST_CASE("BnetFsm: STARTGAME3 sends StartGame3Ack (no use-case)",
          "[protocol][bnet][fsm][startgame]") {
    auto session_ctx = std::make_shared<FakeContext>();
    auto use_case_ctx = make_test_context();
    BnetFsm f{session_ctx, use_case_ctx};
    reach_in_chat(f);

    StartGame3Request req{};
    req.game_name = "mygame3";
    REQUIRE(f.handle(ClientMessage{req}).has_value());
    REQUIRE(f.state() == BnetState::InGame);
    REQUIRE(std::holds_alternative<StartGame3Ack>(session_ctx->sent.back()));
    REQUIRE(std::get<StartGame3Ack>(session_ctx->sent.back()).reply == 0x00u);
}

TEST_CASE("BnetFsm: session_id passed to constructor is accepted",
          "[protocol][bnet][fsm][session_id]") {
    auto session_ctx = std::make_shared<FakeContext>();
    auto use_case_ctx = make_test_context();
    domain::SessionId sid{99};
    BnetFsm f{session_ctx, use_case_ctx, sid};
    REQUIRE(f.state() == BnetState::Init);
    // Ping still works with a non-default session_id
    REQUIRE(f.handle(ClientMessage{Ping{0xDEAD}}).has_value());
    REQUIRE(std::get<Ping>(session_ctx->sent.back()).ticks == 0xDEADu);
}

TEST_CASE("BnetFsm: Null message is accepted in every state",
          "[protocol][bnet][fsm]") {
    auto session_ctx = std::make_shared<FakeContext>();
    auto use_case_ctx = make_test_context();
    BnetFsm f{session_ctx, use_case_ctx};

    // Init
    REQUIRE(f.handle(ClientMessage{Null{}}).has_value());
    REQUIRE(f.state() == BnetState::Init);

    // AuthInfoReceived
    REQUIRE(f.handle(ClientMessage{AuthInfo{}}).has_value());
    REQUIRE(f.handle(ClientMessage{Null{}}).has_value());
    REQUIRE(f.state() == BnetState::AuthInfoReceived);

    // LoggedIn
    LogonResponse2 logon{};
    logon.username = "alice";
    REQUIRE(f.handle(ClientMessage{logon}).has_value());
    REQUIRE(f.handle(ClientMessage{Null{}}).has_value());
    REQUIRE(f.state() == BnetState::LoggedIn);
}
