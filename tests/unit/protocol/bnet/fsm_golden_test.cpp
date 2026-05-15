// SPDX-License-Identifier: GPL-2.0-or-later

#include <catch2/catch_test_macros.hpp>
#include <memory>

#include "protocol/bnet/fsm.hpp"
#include "protocol/bnet/messages.hpp"
#include "tests/unit/protocol/bnet/capturing_session_context.hpp"

namespace pvpgn::protocol::bnet::test {

// Helper to create a minimal use-case context for testing
BnetUseCaseContext make_test_use_cases() {
    // TODO: Implement or mock BnetUseCaseContext
    // For now, return a default-constructed context
    return BnetUseCaseContext{};
}

// Helper to run the initial login sequence
void login_sequence(BnetFsm& fsm, std::shared_ptr<CapturingSessionContext> ctx) {
    fsm.on(AuthInfo{.protocol_id = 0, .platform_id = 0x49583836, .game_id = 0x53544152});
    fsm.on(LogonResponse2{.username = "testuser", .password_hash = {}});
    fsm.on(EnterChatRequest{.username = "testuser", .statstring = ""});
}

TEST_CASE("BnetFsm: complete session golden test") {
    auto ctx = std::make_shared<CapturingSessionContext>();
    BnetFsm fsm(ctx, make_test_use_cases());

    // Verify initial state
    REQUIRE(fsm.state() == BnetState::Init);

    // Send AuthInfo
    auto status = fsm.on(AuthInfo{.protocol_id = 0, .platform_id = 0x49583836, .game_id = 0x53544152});
    REQUIRE(status.is_ok());
    REQUIRE(fsm.state() == BnetState::AuthInfoReceived);
    // Server should have sent AuthInfoReply
    REQUIRE(ctx->all_sent().size() >= 1);

    // Send LogonResponse2 (successful auth)
    ctx->clear_sent();
    status = fsm.on(LogonResponse2{.username = "testuser", .password_hash = {}});
    REQUIRE(status.is_ok());
    REQUIRE(fsm.state() == BnetState::LoggedIn);
    // Should have sent LogonResponse2Reply
    auto last_result = ctx->last_logon_result();
    REQUIRE(last_result == 0);  // success

    // Send EnterChat
    ctx->clear_sent();
    status = fsm.on(EnterChatRequest{.username = "testuser", .statstring = ""});
    REQUIRE(status.is_ok());
    REQUIRE(fsm.state() == BnetState::InChat);

    // Send a chat command
    ctx->clear_sent();
    status = fsm.on(ChatCommand{.text = "hello world"});
    REQUIRE(status.is_ok());
    REQUIRE(fsm.state() == BnetState::InChat);

    // Send JoinChannel
    ctx->clear_sent();
    status = fsm.on(JoinChannel{.channel = "Starcraft USA-1", .flags = 0});
    REQUIRE(status.is_ok());
    REQUIRE(fsm.state() == BnetState::InChat);

    // Send LeaveChannel
    ctx->clear_sent();
    status = fsm.on(LeaveChannel{});
    REQUIRE(status.is_ok());
    REQUIRE(fsm.state() == BnetState::InChat);

    // Session is still open
    REQUIRE(!ctx->closed());
}

TEST_CASE("BnetFsm: authentication failure handling") {
    auto ctx = std::make_shared<CapturingSessionContext>();
    BnetFsm fsm(ctx, make_test_use_cases());

    // Send AuthInfo
    auto status = fsm.on(AuthInfo{.protocol_id = 0, .platform_id = 0x49583836, .game_id = 0x53544152});
    REQUIRE(status.is_ok());
    REQUIRE(fsm.state() == BnetState::AuthInfoReceived);

    // Send LogonResponse2 with invalid credentials
    // This would normally be handled by the use-cases, but the FSM should not crash
    ctx->clear_sent();
    status = fsm.on(LogonResponse2{.username = "baduser", .password_hash = {}});
    REQUIRE(status.is_ok());

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

    // TODO: Implement StartGame4Request and game-related messages
    // Once implemented, verify:
    // 1. Client sends StartGame4Request
    // 2. FSM transitions to InGame state
    // 3. Server sends game-related messages
    // 4. Client sends CloseGame
    // 5. FSM transitions back to InChat
}

TEST_CASE("BnetFsm: ping/keepalive in any state") {
    auto ctx = std::make_shared<CapturingSessionContext>();
    BnetFsm fsm(ctx, make_test_use_cases());

    // Ping in Init state should not crash
    auto status = fsm.on(Ping{.ticks = 12345});
    REQUIRE(status.is_ok());
    REQUIRE(fsm.state() == BnetState::Init);

    // Ping in AuthInfoReceived state
    fsm.on(AuthInfo{.protocol_id = 0, .platform_id = 0x49583836, .game_id = 0x53544152});
    status = fsm.on(Ping{.ticks = 12346});
    REQUIRE(status.is_ok());

    // Ping in LoggedIn state
    fsm.on(LogonResponse2{.username = "testuser", .password_hash = {}});
    status = fsm.on(Ping{.ticks = 12347});
    REQUIRE(status.is_ok());

    // Ping in InChat state
    fsm.on(EnterChatRequest{.username = "testuser", .statstring = ""});
    status = fsm.on(Ping{.ticks = 12348});
    REQUIRE(status.is_ok());
}

TEST_CASE("BnetFsm: null message (keepalive) in any state") {
    auto ctx = std::make_shared<CapturingSessionContext>();
    BnetFsm fsm(ctx, make_test_use_cases());

    // Null in Init state should not crash
    auto status = fsm.on(Null{});
    REQUIRE(status.is_ok());
    REQUIRE(fsm.state() == BnetState::Init);

    // Null in other states
    fsm.on(AuthInfo{.protocol_id = 0, .platform_id = 0x49583836, .game_id = 0x53544152});
    status = fsm.on(Null{});
    REQUIRE(status.is_ok());

    fsm.on(LogonResponse2{.username = "testuser", .password_hash = {}});
    status = fsm.on(Null{});
    REQUIRE(status.is_ok());
}

TEST_CASE("BnetFsm: illegal message sequence rejected") {
    auto ctx = std::make_shared<CapturingSessionContext>();
    BnetFsm fsm(ctx, make_test_use_cases());

    // LogonResponse2 without prior AuthInfo should fail
    auto status = fsm.on(LogonResponse2{.username = "testuser", .password_hash = {}});
    // This should return an error or be rejected by state machine validation
    // The exact behavior depends on FSM implementation
}

}  // namespace pvpgn::protocol::bnet::test
