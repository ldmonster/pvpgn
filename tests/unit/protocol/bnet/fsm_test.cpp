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
