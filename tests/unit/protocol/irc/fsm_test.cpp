// SPDX-License-Identifier: GPL-2.0-or-later
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "protocol/irc/fsm.hpp"

using namespace pvpgn;
using namespace pvpgn::protocol::irc;

namespace {

class FakeContext : public ISessionContext {
public:
    std::vector<Message> sent;
    bool closed = false;

    core::Status<> send(const Message& m) override {
        sent.push_back(m);
        return core::ok();
    }
    std::string_view server_name() const noexcept override {
        return "pvpgn.test";
    }
    void close() override { closed = true; }
};

Message msg(std::string cmd, std::vector<std::string> params = {}) {
    return Message{"", std::move(cmd), std::move(params)};
}

}  // namespace

TEST_CASE("IrcFsm: NICK alone keeps Greeting state", "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    REQUIRE(f.handle(msg("NICK", {"alice"})).has_value());
    REQUIRE(f.state() == IrcState::Greeting);
    REQUIRE(ctx.sent.empty());
}

TEST_CASE("IrcFsm: NICK+USER completes registration with 001 welcome",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    REQUIRE(f.handle(msg("NICK", {"alice"})).has_value());
    REQUIRE(f.handle(msg("USER", {"alice", "0", "*", "Alice"})).has_value());
    REQUIRE(f.state() == IrcState::Registered);
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].prefix  == "pvpgn.test");
    REQUIRE(ctx.sent[0].command == "001");
    REQUIRE(ctx.sent[0].params.front() == "alice");
}

TEST_CASE("IrcFsm: NICK with no nickname emits 431",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    REQUIRE(f.handle(msg("NICK", {})).has_value());
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "431");
}

TEST_CASE("IrcFsm: USER with too few params emits 461",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    REQUIRE(f.handle(msg("USER", {"a"})).has_value());
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "461");
}

TEST_CASE("IrcFsm: PING is answered with PONG",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    REQUIRE(f.handle(msg("PING", {"cookie"})).has_value());
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "PONG");
    REQUIRE(ctx.sent[0].params.back() == "cookie");
}

TEST_CASE("IrcFsm: PRIVMSG before registration yields 451",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    REQUIRE(f.handle(msg("PRIVMSG", {"#x", "hi"})).has_value());
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "451");
}

TEST_CASE("IrcFsm: JOIN echoes membership + 366 EndOfNames",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    REQUIRE(f.handle(msg("NICK", {"alice"})).has_value());
    REQUIRE(f.handle(msg("USER", {"alice", "0", "*", "Alice"})).has_value());
    ctx.sent.clear();
    REQUIRE(f.handle(msg("JOIN", {"#pvpgn"})).has_value());
    REQUIRE(f.state() == IrcState::InChannel);
    REQUIRE(std::string{f.channel()} == "#pvpgn");
    REQUIRE(ctx.sent.size() == 2);
    REQUIRE(ctx.sent[0].command == "JOIN");
    REQUIRE(ctx.sent[0].prefix  == "alice");
    REQUIRE(ctx.sent[1].command == "366");
}

TEST_CASE("IrcFsm: unknown command yields 421",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    REQUIRE(f.handle(msg("WUBWUB", {"x"})).has_value());
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "421");
}

TEST_CASE("IrcFsm: QUIT closes the session", "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    REQUIRE(f.handle(msg("QUIT", {"bye"})).has_value());
    REQUIRE(f.state() == IrcState::Closing);
    REQUIRE(ctx.closed);
}
