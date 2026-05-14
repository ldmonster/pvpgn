// SPDX-License-Identifier: GPL-2.0-or-later
#include <string>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>

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

}  // namespace

TEST_CASE("BnetFsm: Ping is mirrored verbatim in any state",
          "[protocol][bnet][fsm]") {
    FakeContext ctx;
    BnetFsm f{ctx};
    REQUIRE(f.handle(ClientMessage{Ping{0xCAFEBABE}}).has_value());
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(std::get<Ping>(ctx.sent[0]).ticks == 0xCAFEBABE);
    REQUIRE(f.state() == BnetState::Init);
}

TEST_CASE("BnetFsm: AUTH_INFO transitions to AuthInfoReceived + AuthCheckReply",
          "[protocol][bnet][fsm]") {
    FakeContext ctx;
    BnetFsm f{ctx};
    REQUIRE(f.handle(ClientMessage{AuthInfo{}}).has_value());
    REQUIRE(f.state() == BnetState::AuthInfoReceived);
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(std::get<AuthCheckReply>(ctx.sent[0]).result == 0u);
}

TEST_CASE("BnetFsm: AUTH_INFO out of order closes session",
          "[protocol][bnet][fsm]") {
    FakeContext ctx;
    BnetFsm f{ctx};
    REQUIRE(f.handle(ClientMessage{AuthInfo{}}).has_value());
    auto s = f.handle(ClientMessage{AuthInfo{}});
    REQUIRE_FALSE(s.has_value());
    REQUIRE(ctx.closed);
    REQUIRE(f.state() == BnetState::Closing);
}

TEST_CASE("BnetFsm: happy path to InChat",
          "[protocol][bnet][fsm]") {
    FakeContext ctx;
    BnetFsm f{ctx};
    REQUIRE(f.handle(ClientMessage{AuthInfo{}}).has_value());

    LogonResponse2 logon{};
    logon.username = "alice";
    REQUIRE(f.handle(ClientMessage{logon}).has_value());
    REQUIRE(f.state() == BnetState::LoggedIn);
    REQUIRE(std::get<LogonResponse2Reply>(ctx.sent.back()).result == 0u);

    EnterChatRequest req{"alice", "PXES"};
    REQUIRE(f.handle(ClientMessage{req}).has_value());
    REQUIRE(f.state() == BnetState::InChat);
    REQUIRE(std::get<EnterChatReply>(ctx.sent.back()).account == "alice");

    REQUIRE(f.handle(ClientMessage{JoinChannel{0, "Op Allstars"}}).has_value());
    REQUIRE(std::get<ChatEvent>(ctx.sent.back()).text == "Op Allstars");

    REQUIRE(f.handle(ClientMessage{ChatCommand{"hello"}}).has_value());
    REQUIRE(std::get<ChatEvent>(ctx.sent.back()).text == "hello");
    REQUIRE(std::get<ChatEvent>(ctx.sent.back()).event_id == 5u);
}

TEST_CASE("BnetFsm: empty username yields login failure (no transition)",
          "[protocol][bnet][fsm]") {
    FakeContext ctx;
    BnetFsm f{ctx};
    REQUIRE(f.handle(ClientMessage{AuthInfo{}}).has_value());
    REQUIRE(f.handle(ClientMessage{LogonResponse2{}}).has_value());
    REQUIRE(f.state() == BnetState::AuthInfoReceived);
    REQUIRE(std::get<LogonResponse2Reply>(ctx.sent.back()).result == 0x01u);
}

TEST_CASE("BnetFsm: JOINCHANNEL before ENTERCHAT closes session",
          "[protocol][bnet][fsm]") {
    FakeContext ctx;
    BnetFsm f{ctx};
    REQUIRE(f.handle(ClientMessage{AuthInfo{}}).has_value());
    LogonResponse2 logon{};
    logon.username = "alice";
    REQUIRE(f.handle(ClientMessage{logon}).has_value());

    auto s = f.handle(ClientMessage{JoinChannel{0, "x"}});
    REQUIRE_FALSE(s.has_value());
    REQUIRE(ctx.closed);
}

TEST_CASE("BnetFsm: STARTGAME1 transitions LoggedIn -> InGame",
          "[protocol][bnet][fsm]") {
    FakeContext ctx;
    BnetFsm f{ctx};
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
    FakeContext ctx;
    BnetFsm f{ctx};
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
    FakeContext ctx;
    BnetFsm f{ctx};
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
    FakeContext ctx;
    BnetFsm f{ctx};
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
    FakeContext ctx;
    BnetFsm f{ctx};
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
    FakeContext ctx;
    BnetFsm f{ctx};
    StartGame1Request req{};
    req.game_name = "g";
    auto s = f.handle(ClientMessage{req});
    REQUIRE_FALSE(s.has_value());
}

TEST_CASE("BnetFsm: CLOSEGAME outside InGame is accepted as no-op",
          "[protocol][bnet][fsm]") {
    FakeContext ctx;
    BnetFsm f{ctx};
    REQUIRE(f.handle(ClientMessage{AuthInfo{}}).has_value());
    LogonResponse2 logon{};
    logon.username = "alice";
    REQUIRE(f.handle(ClientMessage{logon}).has_value());
    REQUIRE(f.state() == BnetState::LoggedIn);
    REQUIRE(f.handle(ClientMessage{CloseGame{}}).has_value());
    REQUIRE(f.state() == BnetState::LoggedIn);
}

