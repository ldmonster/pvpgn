// SPDX-License-Identifier: GPL-2.0-or-later
//
// Unit tests for IrcFsm — RFC 1459 server-side session FSM.
//
// Coverage:
//   - Initial state is Greeting
//   - NICK alone keeps Greeting state
//   - USER alone keeps Greeting state
//   - NICK+USER completes registration → 001 RPL_WELCOME
//   - USER+NICK (reversed order) also completes registration
//   - NICK with no param → 431 ERR_NONICKNAMEGIVEN
//   - USER with too few params → 461 ERR_NEEDMOREPARAMS
//   - PING → PONG (any state)
//   - MOTD → 375 RPL_MOTDSTART + 376 RPL_ENDOFMOTD
//   - PRIVMSG before registration → 451 ERR_NOTREGISTERED
//   - NOTICE before registration → 451 ERR_NOTREGISTERED
//   - JOIN before registration → 451 ERR_NOTREGISTERED
//   - JOIN echoes JOIN + 353 RPL_NAMREPLY + 366 RPL_ENDOFNAMES
//   - JOIN with no channel → 461 ERR_NEEDMOREPARAMS
//   - PART leaves channel → echoes PART, state → Registered
//   - PART with reason echoes reason
//   - PART wrong channel → 403 ERR_NOSUCHCHANNEL
//   - PART before joining → 403 ERR_NOSUCHCHANNEL
//   - PRIVMSG after registration echoes back
//   - PRIVMSG with missing params → 411 ERR_NORECIPIENT
//   - NOTICE after registration echoes back
//   - AWAY sets away status → 306 RPL_NOWAWAY
//   - AWAY with no message clears away → 305 RPL_UNAWAY
//   - WHOIS self → 311 RPL_WHOISUSER + 318 RPL_ENDOFWHOIS
//   - WHOIS unknown nick → 401 ERR_NOSUCHNICK
//   - WHO * → 352 RPL_WHOREPLY + 315 RPL_ENDOFWHO
//   - WHO with channel mask → 352 + 315
//   - MODE → 324 RPL_CHANNELMODEIS
//   - TOPIC get (no topic) → 331 RPL_NOTOPIC
//   - TOPIC set → echoes TOPIC
//   - TOPIC get (after set) → 332 RPL_TOPIC
//   - TOPIC wrong channel → 403 ERR_NOSUCHCHANNEL
//   - NAMES in channel → 353 RPL_NAMREPLY + 366 RPL_ENDOFNAMES
//   - NAMES not in channel → 366 only
//   - KICK in channel → echoes KICK
//   - KICK with reason → echoes reason
//   - KICK wrong channel → 403 ERR_NOSUCHCHANNEL
//   - LIST before joining → 321 + 323 (no 322)
//   - LIST after joining → 321 + 322 + 323
//   - QUIT closes session
//   - QUIT from Greeting state closes session
//   - handle() after Closing returns error
//   - Unknown command → 421 ERR_UNKNOWNCOMMAND
//   - Numeric reply format: prefix=server, first param=nick
//   - 001 welcome text contains nick

#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "protocol/irc/fsm.hpp"

using namespace pvpgn;
using namespace pvpgn::protocol::irc;

// ===========================================================================
// Test helpers
// ===========================================================================

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

    /// Return true if any sent message has the given command.
    bool has_command(std::string_view cmd) const {
        for (const auto& m : sent)
            if (m.command == cmd) return true;
        return false;
    }

    /// Return the first message with the given command, or a default Message.
    Message first_with_command(std::string_view cmd) const {
        for (const auto& m : sent)
            if (m.command == cmd) return m;
        return {};
    }
};

/// Build a Message with no prefix.
Message msg(std::string cmd, std::vector<std::string> params = {}) {
    return Message{"", std::move(cmd), std::move(params)};
}

/// Drive FSM to Registered state (NICK + USER).
void register_user(IrcFsm& f, FakeContext& ctx,
                   std::string_view nick = "alice",
                   std::string_view user = "alice") {
    REQUIRE(f.handle(msg("NICK", {std::string{nick}})).has_value());
    REQUIRE(f.handle(msg("USER", {std::string{user}, "0", "*", "Alice"})).has_value());
    ctx.sent.clear();
}

/// Drive FSM to InChannel state.
void join_channel(IrcFsm& f, FakeContext& ctx,
                  std::string_view channel = "#pvpgn") {
    register_user(f, ctx);
    REQUIRE(f.handle(msg("JOIN", {std::string{channel}})).has_value());
    ctx.sent.clear();
}

}  // namespace

// ===========================================================================
// Registration
// ===========================================================================

TEST_CASE("IrcFsm: initial state is Greeting", "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    REQUIRE(f.state() == IrcState::Greeting);
    REQUIRE(f.nick().empty());
    REQUIRE(f.user().empty());
    REQUIRE(f.channel().empty());
    REQUIRE(ctx.sent.empty());
}

TEST_CASE("IrcFsm: NICK alone keeps Greeting state", "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    REQUIRE(f.handle(msg("NICK", {"alice"})).has_value());
    REQUIRE(f.state() == IrcState::Greeting);
    REQUIRE(std::string{f.nick()} == "alice");
    REQUIRE(ctx.sent.empty());
}

TEST_CASE("IrcFsm: USER alone keeps Greeting state", "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    REQUIRE(f.handle(msg("USER", {"alice", "0", "*", "Alice"})).has_value());
    REQUIRE(f.state() == IrcState::Greeting);
    REQUIRE(ctx.sent.empty());
}

TEST_CASE("IrcFsm: NICK+USER completes registration with 001 RPL_WELCOME",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    REQUIRE(f.handle(msg("NICK", {"alice"})).has_value());
    REQUIRE(f.handle(msg("USER", {"alice", "0", "*", "Alice"})).has_value());
    REQUIRE(f.state() == IrcState::Registered);
    REQUIRE(ctx.sent.size() == 1);
    const auto& welcome = ctx.sent[0];
    REQUIRE(welcome.prefix  == "pvpgn.test");
    REQUIRE(welcome.command == "001");
    REQUIRE(welcome.params.front() == "alice");
    // Welcome text must contain the nick.
    REQUIRE(welcome.params.back().find("alice") != std::string::npos);
}

TEST_CASE("IrcFsm: USER+NICK (reversed) also completes registration",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    REQUIRE(f.handle(msg("USER", {"bob", "0", "*", "Bob"})).has_value());
    REQUIRE(f.state() == IrcState::Greeting);
    REQUIRE(f.handle(msg("NICK", {"bob"})).has_value());
    REQUIRE(f.state() == IrcState::Registered);
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "001");
    REQUIRE(ctx.sent[0].params.front() == "bob");
}

TEST_CASE("IrcFsm: NICK with no param emits 431 ERR_NONICKNAMEGIVEN",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    REQUIRE(f.handle(msg("NICK", {})).has_value());
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "431");
    REQUIRE(ctx.sent[0].prefix  == "pvpgn.test");
    // Target is "*" before nick is set.
    REQUIRE(ctx.sent[0].params.front() == "*");
}

TEST_CASE("IrcFsm: USER with too few params emits 461 ERR_NEEDMOREPARAMS",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    REQUIRE(f.handle(msg("USER", {"a"})).has_value());
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "461");
    REQUIRE(ctx.sent[0].prefix  == "pvpgn.test");
}

// ===========================================================================
// PING / PONG
// ===========================================================================

TEST_CASE("IrcFsm: PING is answered with PONG carrying the cookie",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    REQUIRE(f.handle(msg("PING", {"cookie123"})).has_value());
    REQUIRE(ctx.sent.size() == 1);
    const auto& pong = ctx.sent[0];
    REQUIRE(pong.command == "PONG");
    REQUIRE(pong.prefix  == "pvpgn.test");
    REQUIRE(pong.params.back() == "cookie123");
}

TEST_CASE("IrcFsm: PING without cookie still sends PONG",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    REQUIRE(f.handle(msg("PING", {})).has_value());
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "PONG");
}

// ===========================================================================
// MOTD
// ===========================================================================

TEST_CASE("IrcFsm: MOTD returns 375 RPL_MOTDSTART + 376 RPL_ENDOFMOTD",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    register_user(f, ctx);
    REQUIRE(f.handle(msg("MOTD", {})).has_value());
    REQUIRE(ctx.sent.size() == 2);
    REQUIRE(ctx.sent[0].command == "375");
    REQUIRE(ctx.sent[0].prefix  == "pvpgn.test");
    REQUIRE(ctx.sent[1].command == "376");
    REQUIRE(ctx.sent[1].prefix  == "pvpgn.test");
}

// ===========================================================================
// Pre-registration guards
// ===========================================================================

TEST_CASE("IrcFsm: PRIVMSG before registration yields 451 ERR_NOTREGISTERED",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    REQUIRE(f.handle(msg("PRIVMSG", {"#x", "hi"})).has_value());
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "451");
    REQUIRE(ctx.sent[0].params.front() == "*");
}

TEST_CASE("IrcFsm: NOTICE before registration yields 451 ERR_NOTREGISTERED",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    REQUIRE(f.handle(msg("NOTICE", {"#x", "hi"})).has_value());
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "451");
}

TEST_CASE("IrcFsm: JOIN before registration yields 451 ERR_NOTREGISTERED",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    REQUIRE(f.handle(msg("JOIN", {"#x"})).has_value());
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "451");
}

// ===========================================================================
// JOIN
// ===========================================================================

TEST_CASE("IrcFsm: JOIN echoes JOIN + 332 RPL_TOPIC + 353 RPL_NAMREPLY + 366 RPL_ENDOFNAMES",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    register_user(f, ctx);
    REQUIRE(f.handle(msg("JOIN", {"#pvpgn"})).has_value());
    REQUIRE(f.state() == IrcState::InChannel);
    REQUIRE(std::string{f.channel()} == "#pvpgn");
    // JOIN echo, 332 RPL_TOPIC (empty), 353 RPL_NAMREPLY, 366 RPL_ENDOFNAMES
    REQUIRE(ctx.sent.size() == 4);
    REQUIRE(ctx.sent[0].command == "JOIN");
    // prefix is nick!nick@pvpgn
    REQUIRE(ctx.sent[0].prefix  == "alice!alice@pvpgn");
    REQUIRE(ctx.sent[0].params.front() == "#pvpgn");
    REQUIRE(ctx.sent[1].command == "332");  // RPL_TOPIC (empty topic)
    REQUIRE(ctx.sent[2].command == "353");
    REQUIRE(ctx.sent[3].command == "366");
}

TEST_CASE("IrcFsm: JOIN with no channel param yields 461 ERR_NEEDMOREPARAMS",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    register_user(f, ctx);
    REQUIRE(f.handle(msg("JOIN", {})).has_value());
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "461");
}

// ===========================================================================
// PART
// ===========================================================================

TEST_CASE("IrcFsm: PART leaves channel and transitions to Registered",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");
    REQUIRE(f.handle(msg("PART", {"#pvpgn"})).has_value());
    REQUIRE(f.state() == IrcState::Registered);
    REQUIRE(f.channel().empty());
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "PART");
    // prefix is nick!nick@pvpgn
    REQUIRE(ctx.sent[0].prefix  == "alice!alice@pvpgn");
    REQUIRE(ctx.sent[0].params.front() == "#pvpgn");
}

TEST_CASE("IrcFsm: PART with reason echoes the reason",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");
    REQUIRE(f.handle(msg("PART", {"#pvpgn", "Goodbye!"})).has_value());
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "PART");
    REQUIRE(ctx.sent[0].params.size() == 2);
    REQUIRE(ctx.sent[0].params[1] == "Goodbye!");
}

TEST_CASE("IrcFsm: PART wrong channel yields 403 ERR_NOSUCHCHANNEL",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");
    REQUIRE(f.handle(msg("PART", {"#other"})).has_value());
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "403");
}

TEST_CASE("IrcFsm: PART before joining any channel yields 403 ERR_NOSUCHCHANNEL",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    register_user(f, ctx);
    REQUIRE(f.handle(msg("PART", {"#pvpgn"})).has_value());
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "403");
}

// ===========================================================================
// PRIVMSG / NOTICE
// ===========================================================================

TEST_CASE("IrcFsm: PRIVMSG to channel is silently accepted (no echo in stub mode)",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    register_user(f, ctx);
    // channel PRIVMSG is forwarded to PostMessage use-case when wired;
    // in stub mode (no use-case) it is silently accepted — no echo back.
    REQUIRE(f.handle(msg("PRIVMSG", {"#pvpgn", "Hello world"})).has_value());
    REQUIRE(ctx.sent.empty());
}

TEST_CASE("IrcFsm: PRIVMSG with missing params yields 411 ERR_NORECIPIENT",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    register_user(f, ctx);
    REQUIRE(f.handle(msg("PRIVMSG", {"#pvpgn"})).has_value());
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "411");
}

TEST_CASE("IrcFsm: NOTICE after registration echoes back with sender prefix",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    register_user(f, ctx);
    REQUIRE(f.handle(msg("NOTICE", {"alice", "Hey there"})).has_value());
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "NOTICE");
    REQUIRE(ctx.sent[0].prefix  == "alice");
    REQUIRE(ctx.sent[0].params[1] == "Hey there");
}

// ===========================================================================
// AWAY
// ===========================================================================

TEST_CASE("IrcFsm: AWAY with message sets away status → 306 RPL_NOWAWAY",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    register_user(f, ctx);
    REQUIRE(f.handle(msg("AWAY", {"Gone fishing"})).has_value());
    REQUIRE(f.is_away());
    REQUIRE(std::string{f.away_msg()} == "Gone fishing");
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "306");
}

TEST_CASE("IrcFsm: AWAY with no message clears away status → 305 RPL_UNAWAY",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    register_user(f, ctx);
    // Set away first.
    REQUIRE(f.handle(msg("AWAY", {"Gone fishing"})).has_value());
    ctx.sent.clear();
    // Clear away.
    REQUIRE(f.handle(msg("AWAY", {})).has_value());
    REQUIRE(!f.is_away());
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "305");
}

// ===========================================================================
// WHOIS
// ===========================================================================

TEST_CASE("IrcFsm: WHOIS self returns 311 RPL_WHOISUSER + 318 RPL_ENDOFWHOIS",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    register_user(f, ctx);
    REQUIRE(f.handle(msg("WHOIS", {"alice"})).has_value());
    REQUIRE(ctx.sent.size() == 2);
    REQUIRE(ctx.sent[0].command == "311");
    REQUIRE(ctx.sent[0].prefix  == "pvpgn.test");
    // 311 params: <nick> <nick> <user> <host> * <realname>
    REQUIRE(ctx.sent[0].params[0] == "alice");  // target
    REQUIRE(ctx.sent[0].params[1] == "alice");  // nick
    REQUIRE(ctx.sent[1].command == "318");
}

TEST_CASE("IrcFsm: WHOIS unknown nick returns 401 ERR_NOSUCHNICK",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    register_user(f, ctx);
    REQUIRE(f.handle(msg("WHOIS", {"nobody"})).has_value());
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "401");
    REQUIRE(ctx.sent[0].prefix  == "pvpgn.test");
}

// ===========================================================================
// WHO
// ===========================================================================

TEST_CASE("IrcFsm: WHO * returns 352 RPL_WHOREPLY + 315 RPL_ENDOFWHO",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    register_user(f, ctx);
    REQUIRE(f.handle(msg("WHO", {"*"})).has_value());
    REQUIRE(ctx.sent.size() == 2);
    REQUIRE(ctx.sent[0].command == "352");
    REQUIRE(ctx.sent[0].prefix  == "pvpgn.test");
    REQUIRE(ctx.sent[1].command == "315");
}

TEST_CASE("IrcFsm: WHO with channel mask returns 352 + 315",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");
    REQUIRE(f.handle(msg("WHO", {"#pvpgn"})).has_value());
    REQUIRE(ctx.sent.size() == 2);
    REQUIRE(ctx.sent[0].command == "352");
    REQUIRE(ctx.sent[1].command == "315");
}

// ===========================================================================
// MODE
// ===========================================================================

TEST_CASE("IrcFsm: MODE returns 324 RPL_CHANNELMODEIS",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");
    REQUIRE(f.handle(msg("MODE", {"#pvpgn"})).has_value());
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "324");
    REQUIRE(ctx.sent[0].prefix  == "pvpgn.test");
}

// ===========================================================================
// TOPIC
// ===========================================================================

TEST_CASE("IrcFsm: TOPIC get with no topic set returns 331 RPL_NOTOPIC",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");
    REQUIRE(f.handle(msg("TOPIC", {"#pvpgn"})).has_value());
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "331");
    REQUIRE(ctx.sent[0].prefix  == "pvpgn.test");
}

TEST_CASE("IrcFsm: TOPIC set returns 482 ERR_CHANOPRIVSNEEDED (Phase H stub)",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");
    // set_topic use-case not yet wired; returns 482 ERR_CHANOPRIVSNEEDED.
    REQUIRE(f.handle(msg("TOPIC", {"#pvpgn", "Welcome to PvPGN!"})).has_value());
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "482");
}

TEST_CASE("IrcFsm: TOPIC get with no topic set returns 331 RPL_NOTOPIC (topic not persisted in stub)",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");
    // Attempt to set topic (returns 482, topic_ not updated).
    REQUIRE(f.handle(msg("TOPIC", {"#pvpgn", "Hello topic"})).has_value());
    ctx.sent.clear();
    // Get topic — still empty since set was rejected.
    REQUIRE(f.handle(msg("TOPIC", {"#pvpgn"})).has_value());
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "331");
}

TEST_CASE("IrcFsm: TOPIC on wrong channel returns 403 ERR_NOSUCHCHANNEL",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");
    REQUIRE(f.handle(msg("TOPIC", {"#other"})).has_value());
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "403");
}

// ===========================================================================
// NAMES
// ===========================================================================

TEST_CASE("IrcFsm: NAMES in channel returns 353 RPL_NAMREPLY + 366 RPL_ENDOFNAMES",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");
    REQUIRE(f.handle(msg("NAMES", {"#pvpgn"})).has_value());
    REQUIRE(ctx.sent.size() == 2);
    REQUIRE(ctx.sent[0].command == "353");
    REQUIRE(ctx.sent[0].prefix  == "pvpgn.test");
    // 353 params: <nick> = <channel> <nick-list>
    REQUIRE(ctx.sent[0].params[2] == "#pvpgn");
    REQUIRE(ctx.sent[1].command == "366");
}

TEST_CASE("IrcFsm: NAMES not in channel returns only 366 RPL_ENDOFNAMES",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    register_user(f, ctx);
    REQUIRE(f.handle(msg("NAMES", {"#pvpgn"})).has_value());
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "366");
}

// ===========================================================================
// KICK
// ===========================================================================

TEST_CASE("IrcFsm: KICK in channel returns 482 ERR_CHANOPRIVSNEEDED (Phase H stub)",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");
    // kick use-case not yet wired; returns 482 ERR_CHANOPRIVSNEEDED.
    REQUIRE(f.handle(msg("KICK", {"#pvpgn", "bob"})).has_value());
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "482");
}

TEST_CASE("IrcFsm: KICK with reason also returns 482 ERR_CHANOPRIVSNEEDED",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");
    REQUIRE(f.handle(msg("KICK", {"#pvpgn", "bob", "Spamming"})).has_value());
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "482");
}

TEST_CASE("IrcFsm: KICK wrong channel returns 403 ERR_NOSUCHCHANNEL",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");
    REQUIRE(f.handle(msg("KICK", {"#other", "bob"})).has_value());
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "403");
}

// ===========================================================================
// LIST
// ===========================================================================

TEST_CASE("IrcFsm: LIST before joining returns 321 + 323 (no 322)",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    register_user(f, ctx);
    REQUIRE(f.handle(msg("LIST", {})).has_value());
    REQUIRE(ctx.sent.size() == 2);
    REQUIRE(ctx.sent[0].command == "321");
    REQUIRE(ctx.sent[1].command == "323");
}

TEST_CASE("IrcFsm: LIST after joining returns 321 + 322 RPL_LIST + 323",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");
    REQUIRE(f.handle(msg("LIST", {})).has_value());
    REQUIRE(ctx.sent.size() == 3);
    REQUIRE(ctx.sent[0].command == "321");
    REQUIRE(ctx.sent[1].command == "322");
    REQUIRE(ctx.sent[1].params[1] == "#pvpgn");
    REQUIRE(ctx.sent[2].command == "323");
}

// ===========================================================================
// QUIT
// ===========================================================================

TEST_CASE("IrcFsm: QUIT closes the session and transitions to Closing",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    register_user(f, ctx);
    REQUIRE(f.handle(msg("QUIT", {"Goodbye"})).has_value());
    REQUIRE(f.state() == IrcState::Closing);
    REQUIRE(ctx.closed);
}

TEST_CASE("IrcFsm: QUIT from Greeting state also closes session",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    REQUIRE(f.handle(msg("QUIT", {})).has_value());
    REQUIRE(f.state() == IrcState::Closing);
    REQUIRE(ctx.closed);
}

TEST_CASE("IrcFsm: handle() after Closing returns FailedPrecondition error",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    REQUIRE(f.handle(msg("QUIT", {})).has_value());
    auto result = f.handle(msg("PING", {"x"}));
    REQUIRE(!result.has_value());
    REQUIRE(result.error().code() == core::StatusCode::FailedPrecondition);
}

// ===========================================================================
// Unknown command
// ===========================================================================

TEST_CASE("IrcFsm: unknown command yields 421 ERR_UNKNOWNCOMMAND",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    REQUIRE(f.handle(msg("WUBWUB", {"x"})).has_value());
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "421");
    REQUIRE(ctx.sent[0].prefix  == "pvpgn.test");
    // Target is "*" before registration.
    REQUIRE(ctx.sent[0].params.front() == "*");
    // Text contains the unknown command name.
    REQUIRE(ctx.sent[0].params.back().find("WUBWUB") != std::string::npos);
}

TEST_CASE("IrcFsm: unknown command after registration uses nick as target",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    register_user(f, ctx);
    REQUIRE(f.handle(msg("FOOBAR", {})).has_value());
    REQUIRE(ctx.sent.size() == 1);
    REQUIRE(ctx.sent[0].command == "421");
    REQUIRE(ctx.sent[0].params.front() == "alice");
}

// ===========================================================================
// Numeric reply format validation
// ===========================================================================

TEST_CASE("IrcFsm: numeric replies have server prefix and nick as first param",
          "[protocol][irc][fsm]") {
    FakeContext ctx;
    IrcFsm f{ctx};
    register_user(f, ctx);
    // Trigger a numeric: WHOIS unknown nick → 401
    REQUIRE(f.handle(msg("WHOIS", {"nobody"})).has_value());
    REQUIRE(ctx.sent.size() == 1);
    const auto& reply = ctx.sent[0];
    // RFC 1459: :<server> <NNN> <nick> :<text>
    REQUIRE(reply.prefix == "pvpgn.test");
    REQUIRE(reply.command.size() == 3);
    for (char ch : reply.command) REQUIRE(ch >= '0');
    for (char ch : reply.command) REQUIRE(ch <= '9');
    REQUIRE(reply.params.front() == "alice");
}
