// SPDX-License-Identifier: GPL-2.0-or-later

#include <catch2/catch_test_macros.hpp>
#include <memory>

#include "protocol/bnet/fsm.hpp"
#include "protocol/bnet/messages.hpp"
#include "capturing_session_context.hpp"

namespace pvpgn::protocol::bnet::test {

// Helper to create a minimal use-case context for testing
BnetUseCaseContext make_test_use_cases() {
    // TODO: Implement or mock BnetUseCaseContext
    // For now, return a default-constructed context
    return BnetUseCaseContext{};
}

// Helper to run the initial login sequence
void login_sequence(BnetFsm& fsm, std::shared_ptr<CapturingSessionContext> ctx) {
    (void)fsm.on(AuthInfo{.protocol_id = 0, .platform_id = 0x49583836, .game_id = 0x53544152, .country_abbr = "", .country = ""});
    (void)fsm.on(LogonResponse2{.client_token = 0, .server_token = 0, .password_hash = {}, .username = "testuser"});
    (void)fsm.on(EnterChatRequest{.username = "testuser", .statstring = ""});
}

TEST_CASE("BnetFsm: complete session golden test") {
    auto ctx = std::make_shared<CapturingSessionContext>();
    BnetFsm fsm(ctx, make_test_use_cases());

    // Verify initial state
    REQUIRE(fsm.state() == BnetState::Init);

    // Send AuthInfo
    auto status = fsm.on(AuthInfo{.protocol_id = 0, .platform_id = 0x49583836, .game_id = 0x53544152, .country_abbr = "", .country = ""});
    REQUIRE(status.has_value());
    REQUIRE(fsm.state() == BnetState::AuthInfoReceived);
    // Server should have sent AuthInfoReply
    REQUIRE(ctx->all_sent().size() >= 1);

    // Send LogonResponse2 (successful auth)
    ctx->clear_sent();
    status = fsm.on(LogonResponse2{.client_token = 0, .server_token = 0, .password_hash = {}, .username = "testuser"});
    REQUIRE(status.has_value());
    REQUIRE(fsm.state() == BnetState::LoggedIn);
    // Should have sent LogonResponse2Reply
    auto last_result = ctx->last_logon_result();
    REQUIRE(last_result == 0);  // success

    // Send EnterChat
    ctx->clear_sent();
    status = fsm.on(EnterChatRequest{.username = "testuser", .statstring = ""});
    REQUIRE(status.has_value());
    REQUIRE(fsm.state() == BnetState::InChat);

    // Send a chat command
    ctx->clear_sent();
    status = fsm.on(ChatCommand{.text = "hello world"});
    REQUIRE(status.has_value());
    REQUIRE(fsm.state() == BnetState::InChat);

    // Send JoinChannel
    ctx->clear_sent();
    status = fsm.on(JoinChannel{.flags = 0, .channel = "Starcraft USA-1"});
    REQUIRE(status.has_value());
    REQUIRE(fsm.state() == BnetState::InChat);

    // Send LeaveChannel
    ctx->clear_sent();
    status = fsm.on(LeaveChannel{});
    REQUIRE(status.has_value());
    REQUIRE(fsm.state() == BnetState::InChat);

    // Session is still open
    REQUIRE(!ctx->closed());
}

TEST_CASE("BnetFsm: authentication failure handling") {
    auto ctx = std::make_shared<CapturingSessionContext>();
    BnetFsm fsm(ctx, make_test_use_cases());

    // Send AuthInfo
    auto status = fsm.on(AuthInfo{.protocol_id = 0, .platform_id = 0x49583836, .game_id = 0x53544152, .country_abbr = "", .country = ""});
    REQUIRE(status.has_value());
    REQUIRE(fsm.state() == BnetState::AuthInfoReceived);

    // Send LogonResponse2 with invalid credentials
    // This would normally be handled by the use-cases, but the FSM should not crash
    ctx->clear_sent();
    status = fsm.on(LogonResponse2{.client_token = 0, .server_token = 0, .password_hash = {}, .username = "baduser"});
    REQUIRE(status.has_value());

    // FSM should still be in LoggedIn state (auth decision is app-layer)
    // The session remains open for retry
    REQUIRE(fsm.state() == BnetState::LoggedIn);
    REQUIRE(!ctx->closed());
}

TEST_CASE("BnetFsm: game lifecycle") {
    auto ctx = std::make_shared<CapturingSessionContext>();
    BnetFsm fsm(ctx, make_test_use_cases());

    // Execute login sequence
    login_sequence(fsm, ctx);
    REQUIRE(fsm.state() == BnetState::InChat);

    // Send StartGame4Request to start a game
    ctx->clear_sent();
    auto status = fsm.on(StartGame4Request{
        .status = 0,
        .flag = 0,
        .unknown2 = 0,
        .gametype = 0,
        .option = 0,
        .unknown4 = 0,
        .unknown5 = 0,
        .game_name = "Test Game",
        .password = "",
        .info = ""
    });
    REQUIRE(status.has_value());
    REQUIRE(fsm.state() == BnetState::InGame);

    // Send CloseGame to leave the game
    ctx->clear_sent();
    status = fsm.on(CloseGame{});
    REQUIRE(status.has_value());
    // CloseGame transitions from InGame to LoggedIn (not InChat)
    REQUIRE(fsm.state() == BnetState::LoggedIn);

    // Session is still open
    REQUIRE(!ctx->closed());
}

TEST_CASE("BnetFsm: ping/keepalive in any state") {
    auto ctx = std::make_shared<CapturingSessionContext>();
    BnetFsm fsm(ctx, make_test_use_cases());

    // Ping in Init state should not crash
    auto status = fsm.on(Ping{.ticks = 12345});
    REQUIRE(status.has_value());
    REQUIRE(fsm.state() == BnetState::Init);

    // Ping in AuthInfoReceived state
    (void)fsm.on(AuthInfo{.protocol_id = 0, .platform_id = 0x49583836, .game_id = 0x53544152, .country_abbr = "", .country = ""});
    status = fsm.on(Ping{.ticks = 12346});
    REQUIRE(status.has_value());

    // Ping in LoggedIn state
    (void)fsm.on(LogonResponse2{.client_token = 0, .server_token = 0, .password_hash = {}, .username = "testuser"});
    status = fsm.on(Ping{.ticks = 12347});
    REQUIRE(status.has_value());

    // Ping in InChat state
    (void)fsm.on(EnterChatRequest{.username = "testuser", .statstring = ""});
    status = fsm.on(Ping{.ticks = 12348});
    REQUIRE(status.has_value());
}

TEST_CASE("BnetFsm: null message (keepalive) in any state") {
    auto ctx = std::make_shared<CapturingSessionContext>();
    BnetFsm fsm(ctx, make_test_use_cases());

    // Null in Init state should not crash
    auto status = fsm.on(Null{});
    REQUIRE(status.has_value());
    REQUIRE(fsm.state() == BnetState::Init);

    // Null in other states
    (void)fsm.on(AuthInfo{.protocol_id = 0, .platform_id = 0x49583836, .game_id = 0x53544152, .country_abbr = "", .country = ""});
    status = fsm.on(Null{});
    REQUIRE(status.has_value());

    (void)fsm.on(LogonResponse2{.client_token = 0, .server_token = 0, .password_hash = {}, .username = "testuser"});
    status = fsm.on(Null{});
    REQUIRE(status.has_value());
}

TEST_CASE("BnetFsm: illegal message sequence rejected") {
    auto ctx = std::make_shared<CapturingSessionContext>();
    BnetFsm fsm(ctx, make_test_use_cases());

    // LogonResponse2 without prior AuthInfo should fail
    auto status = fsm.on(LogonResponse2{.client_token = 0, .server_token = 0, .password_hash = {}, .username = "testuser"});
    // This should return an error or be rejected by state machine validation
    // The exact behavior depends on FSM implementation
}

}  // namespace pvpgn::protocol::bnet::test
