// SPDX-License-Identifier: GPL-2.0-or-later
//
// IrcFsm Channel Operation Tests
//
// Covers:
//   - JOIN #channel: client joins, server sends JOIN echo + 332 + 353 + 366
//   - PART #channel: client parts, server sends PART echo
//   - PRIVMSG #channel :message: channel message accepted
//   - PRIVMSG nick :message: private message → 401
//   - LIST: server responds with 321/322/323
//   - TOPIC #channel: get topic → 331/332
//   - TOPIC #channel :new topic: set topic → 482 (no operator)
//   - KICK #channel nick: → 482 (no operator)
//   - NAMES #channel: → 353 + 366
//
// These tests exercise the skeleton (no use-case) mode of IrcFsm.
// The existing fsm_test.cpp covers the same commands; this file adds
// additional edge-case and scenario coverage for the channel layer.

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

class FakeIrcCtx : public ISessionContext {
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

    bool has_command(std::string_view cmd) const {
        for (const auto& m : sent)
            if (m.command == cmd) return true;
        return false;
    }

    Message first_with_command(std::string_view cmd) const {
        for (const auto& m : sent)
            if (m.command == cmd) return m;
        return {};
    }

    std::size_t count_with_command(std::string_view cmd) const {
        std::size_t n = 0;
        for (const auto& m : sent)
            if (m.command == cmd) ++n;
        return n;
    }
};

/// Build a Message with no prefix.
Message msg(std::string cmd, std::vector<std::string> params = {}) {
    return Message{"", std::move(cmd), std::move(params)};
}

/// Drive FSM to Registered state (NICK + USER).
void register_user(IrcFsm& f, FakeIrcCtx& ctx,
                   std::string_view nick = "alice",
                   std::string_view user = "alice") {
    REQUIRE(f.handle(msg("NICK", {std::string{nick}})).has_value());
    REQUIRE(f.handle(msg("USER", {std::string{user}, "0", "*", "Alice"})).has_value());
    ctx.sent.clear();
}

/// Drive FSM to InChannel state.
void join_channel(IrcFsm& f, FakeIrcCtx& ctx,
                  std::string_view channel = "#pvpgn") {
    register_user(f, ctx);
    REQUIRE(f.handle(msg("JOIN", {std::string{channel}})).has_value());
    ctx.sent.clear();
}

}  // namespace

// ===========================================================================
// JOIN #channel — JOIN echo + 332 + 353 + 366
// ===========================================================================

TEST_CASE("IrcFsm R308: JOIN sends JOIN echo with nick prefix",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    register_user(f, ctx, "alice");

    REQUIRE(f.handle(msg("JOIN", {"#pvpgn"})).has_value());

    REQUIRE(ctx.has_command("JOIN"));
    const auto& join_msg = ctx.first_with_command("JOIN");
    REQUIRE(join_msg.prefix == "alice!alice@pvpgn");
    REQUIRE(join_msg.params.front() == "#pvpgn");
}

TEST_CASE("IrcFsm R308: JOIN sends 332 RPL_TOPIC",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    register_user(f, ctx, "alice");

    REQUIRE(f.handle(msg("JOIN", {"#pvpgn"})).has_value());

    REQUIRE(ctx.has_command("332"));
    const auto& topic_msg = ctx.first_with_command("332");
    REQUIRE(topic_msg.prefix == "pvpgn.test");
}

TEST_CASE("IrcFsm R308: JOIN sends 353 RPL_NAMREPLY",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    register_user(f, ctx, "alice");

    REQUIRE(f.handle(msg("JOIN", {"#pvpgn"})).has_value());

    REQUIRE(ctx.has_command("353"));
    const auto& names_msg = ctx.first_with_command("353");
    REQUIRE(names_msg.prefix == "pvpgn.test");
    // 353 params: <nick> = <channel> <nick-list>
    REQUIRE(names_msg.params[2] == "#pvpgn");
}

TEST_CASE("IrcFsm R308: JOIN sends 366 RPL_ENDOFNAMES",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    register_user(f, ctx, "alice");

    REQUIRE(f.handle(msg("JOIN", {"#pvpgn"})).has_value());

    REQUIRE(ctx.has_command("366"));
}

TEST_CASE("IrcFsm R308: JOIN sends exactly 4 messages (JOIN+332+353+366)",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    register_user(f, ctx, "alice");

    REQUIRE(f.handle(msg("JOIN", {"#pvpgn"})).has_value());

    REQUIRE(ctx.sent.size() == 4u);
    REQUIRE(ctx.sent[0].command == "JOIN");
    REQUIRE(ctx.sent[1].command == "332");
    REQUIRE(ctx.sent[2].command == "353");
    REQUIRE(ctx.sent[3].command == "366");
}

TEST_CASE("IrcFsm R308: JOIN transitions FSM to InChannel state",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    register_user(f, ctx, "alice");

    REQUIRE(f.handle(msg("JOIN", {"#pvpgn"})).has_value());

    REQUIRE(f.state() == IrcState::InChannel);
    REQUIRE(std::string{f.channel()} == "#pvpgn");
}

TEST_CASE("IrcFsm R308: JOIN with different channel name stores correct channel",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    register_user(f, ctx, "bob");

    REQUIRE(f.handle(msg("JOIN", {"#diablo"})).has_value());

    REQUIRE(f.state() == IrcState::InChannel);
    REQUIRE(std::string{f.channel()} == "#diablo");
    const auto& join_msg = ctx.first_with_command("JOIN");
    REQUIRE(join_msg.params.front() == "#diablo");
}

// ===========================================================================
// PART #channel — PART echo
// ===========================================================================

TEST_CASE("IrcFsm R308: PART sends PART echo with nick prefix",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");

    REQUIRE(f.handle(msg("PART", {"#pvpgn"})).has_value());

    REQUIRE(ctx.has_command("PART"));
    const auto& part_msg = ctx.first_with_command("PART");
    REQUIRE(part_msg.prefix == "alice!alice@pvpgn");
    REQUIRE(part_msg.params.front() == "#pvpgn");
}

TEST_CASE("IrcFsm R308: PART transitions FSM back to Registered",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");

    REQUIRE(f.handle(msg("PART", {"#pvpgn"})).has_value());

    REQUIRE(f.state() == IrcState::Registered);
    REQUIRE(f.channel().empty());
}

TEST_CASE("IrcFsm R308: PART with reason echoes the reason",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");

    REQUIRE(f.handle(msg("PART", {"#pvpgn", "See you later!"})).has_value());

    const auto& part_msg = ctx.first_with_command("PART");
    REQUIRE(part_msg.params.size() == 2u);
    REQUIRE(part_msg.params[1] == "See you later!");
}

TEST_CASE("IrcFsm R308: PART wrong channel returns 403 ERR_NOSUCHCHANNEL",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");

    REQUIRE(f.handle(msg("PART", {"#other"})).has_value());

    REQUIRE(ctx.has_command("403"));
    REQUIRE(!ctx.has_command("PART"));
}

TEST_CASE("IrcFsm R308: PART before joining returns 403 ERR_NOSUCHCHANNEL",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    register_user(f, ctx);

    REQUIRE(f.handle(msg("PART", {"#pvpgn"})).has_value());

    REQUIRE(ctx.has_command("403"));
}

// ===========================================================================
// PRIVMSG #channel :message — channel message accepted
// ===========================================================================

TEST_CASE("IrcFsm R308: PRIVMSG to channel in InChannel state is accepted silently",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");

    REQUIRE(f.handle(msg("PRIVMSG", {"#pvpgn", "Hello everyone!"})).has_value());

    // In stub mode: no echo, no error
    REQUIRE(ctx.sent.empty());
    REQUIRE(!ctx.closed);
}

TEST_CASE("IrcFsm R308: PRIVMSG to channel in Registered state is accepted silently",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    register_user(f, ctx);

    // PRIVMSG to channel without being in channel — stub accepts silently
    REQUIRE(f.handle(msg("PRIVMSG", {"#pvpgn", "Hello"})).has_value());

    REQUIRE(ctx.sent.empty());
}

TEST_CASE("IrcFsm R308: PRIVMSG with missing message param returns 411",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");

    REQUIRE(f.handle(msg("PRIVMSG", {"#pvpgn"})).has_value());

    REQUIRE(ctx.has_command("411"));
}

TEST_CASE("IrcFsm R308: PRIVMSG before registration returns 451",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};

    REQUIRE(f.handle(msg("PRIVMSG", {"#pvpgn", "hello"})).has_value());

    REQUIRE(ctx.has_command("451"));
}

// ===========================================================================
// PRIVMSG nick :message — private message → 401
// ===========================================================================

TEST_CASE("IrcFsm R308: PRIVMSG to nick returns 401 ERR_NOSUCHNICK",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");

    REQUIRE(f.handle(msg("PRIVMSG", {"bob", "Hey Bob!"})).has_value());

    REQUIRE(ctx.has_command("401"));
}

TEST_CASE("IrcFsm R308: PRIVMSG to nick 401 reply contains target nick",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");

    REQUIRE(f.handle(msg("PRIVMSG", {"charlie", "Hi!"})).has_value());

    const auto& reply = ctx.first_with_command("401");
    REQUIRE(reply.prefix == "pvpgn.test");
    // make_numeric: params[0]=target(=nick_), params[1]=text("charlie :No such nick")
    // The target nick appears at the start of params[1]
    REQUIRE(reply.params[1].find("charlie") != std::string::npos);
}

TEST_CASE("IrcFsm R308: PRIVMSG to nick in Registered state returns 401",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    register_user(f, ctx, "alice");

    REQUIRE(f.handle(msg("PRIVMSG", {"bob", "Hello"})).has_value());

    REQUIRE(ctx.has_command("401"));
}

// ===========================================================================
// LIST — 321/322/323
// ===========================================================================

TEST_CASE("IrcFsm R308: LIST in Registered state returns 321 + 323",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    register_user(f, ctx);

    REQUIRE(f.handle(msg("LIST", {})).has_value());

    REQUIRE(ctx.has_command("321"));
    REQUIRE(ctx.has_command("323"));
    // No 322 when not in a channel
    REQUIRE(!ctx.has_command("322"));
}

TEST_CASE("IrcFsm R308: LIST in InChannel state returns 321 + 322 + 323",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");

    REQUIRE(f.handle(msg("LIST", {})).has_value());

    REQUIRE(ctx.has_command("321"));
    REQUIRE(ctx.has_command("322"));
    REQUIRE(ctx.has_command("323"));
}

TEST_CASE("IrcFsm R308: LIST 322 entry contains channel name",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");

    REQUIRE(f.handle(msg("LIST", {})).has_value());

    const auto& list_entry = ctx.first_with_command("322");
    REQUIRE(list_entry.params[1] == "#pvpgn");
}

TEST_CASE("IrcFsm R308: LIST 321 has server prefix",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    register_user(f, ctx);

    REQUIRE(f.handle(msg("LIST", {})).has_value());

    const auto& start = ctx.first_with_command("321");
    REQUIRE(start.prefix == "pvpgn.test");
}

TEST_CASE("IrcFsm R308: LIST order is 321 then 322 then 323",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");

    REQUIRE(f.handle(msg("LIST", {})).has_value());

    REQUIRE(ctx.sent.size() == 3u);
    REQUIRE(ctx.sent[0].command == "321");
    REQUIRE(ctx.sent[1].command == "322");
    REQUIRE(ctx.sent[2].command == "323");
}

// ===========================================================================
// TOPIC #channel — get topic → 331/332
// ===========================================================================

TEST_CASE("IrcFsm R308: TOPIC get with no topic returns 331 RPL_NOTOPIC",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");

    REQUIRE(f.handle(msg("TOPIC", {"#pvpgn"})).has_value());

    REQUIRE(ctx.has_command("331"));
    REQUIRE(ctx.sent.size() == 1u);
    REQUIRE(ctx.sent[0].prefix == "pvpgn.test");
}

TEST_CASE("IrcFsm R308: TOPIC get on wrong channel returns 403",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");

    REQUIRE(f.handle(msg("TOPIC", {"#other"})).has_value());

    REQUIRE(ctx.has_command("403"));
    REQUIRE(!ctx.has_command("331"));
    REQUIRE(!ctx.has_command("332"));
}

TEST_CASE("IrcFsm R308: TOPIC get after failed set still returns 331",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");

    // Attempt to set topic (returns 482, topic not updated)
    REQUIRE(f.handle(msg("TOPIC", {"#pvpgn", "New topic"})).has_value());
    ctx.sent.clear();

    // Get topic — still empty
    REQUIRE(f.handle(msg("TOPIC", {"#pvpgn"})).has_value());
    REQUIRE(ctx.has_command("331"));
}

// ===========================================================================
// TOPIC #channel :new topic — set topic → 482 (no operator)
// ===========================================================================

TEST_CASE("IrcFsm R308: TOPIC set returns 482 ERR_CHANOPRIVSNEEDED",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");

    REQUIRE(f.handle(msg("TOPIC", {"#pvpgn", "Welcome to PvPGN!"})).has_value());

    REQUIRE(ctx.has_command("482"));
    REQUIRE(ctx.sent.size() == 1u);
}

TEST_CASE("IrcFsm R308: TOPIC set 482 reply has server prefix",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");

    REQUIRE(f.handle(msg("TOPIC", {"#pvpgn", "test topic"})).has_value());

    const auto& reply = ctx.first_with_command("482");
    REQUIRE(reply.prefix == "pvpgn.test");
}

TEST_CASE("IrcFsm R308: TOPIC set on wrong channel returns 403",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");

    REQUIRE(f.handle(msg("TOPIC", {"#other", "new topic"})).has_value());

    REQUIRE(ctx.has_command("403"));
    REQUIRE(!ctx.has_command("482"));
}

// ===========================================================================
// KICK #channel nick — → 482 (no operator)
// ===========================================================================

TEST_CASE("IrcFsm R308: KICK in channel returns 482 ERR_CHANOPRIVSNEEDED",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");

    REQUIRE(f.handle(msg("KICK", {"#pvpgn", "bob"})).has_value());

    REQUIRE(ctx.has_command("482"));
    REQUIRE(ctx.sent.size() == 1u);
}

TEST_CASE("IrcFsm R308: KICK with reason also returns 482",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");

    REQUIRE(f.handle(msg("KICK", {"#pvpgn", "bob", "Spamming"})).has_value());

    REQUIRE(ctx.has_command("482"));
}

TEST_CASE("IrcFsm R308: KICK 482 reply has server prefix",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");

    REQUIRE(f.handle(msg("KICK", {"#pvpgn", "bob"})).has_value());

    const auto& reply = ctx.first_with_command("482");
    REQUIRE(reply.prefix == "pvpgn.test");
}

TEST_CASE("IrcFsm R308: KICK wrong channel returns 403 ERR_NOSUCHCHANNEL",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");

    REQUIRE(f.handle(msg("KICK", {"#other", "bob"})).has_value());

    REQUIRE(ctx.has_command("403"));
    REQUIRE(!ctx.has_command("482"));
}

// ===========================================================================
// NAMES #channel — → 353 + 366
// ===========================================================================

TEST_CASE("IrcFsm R308: NAMES in channel returns 353 RPL_NAMREPLY",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");

    REQUIRE(f.handle(msg("NAMES", {"#pvpgn"})).has_value());

    REQUIRE(ctx.has_command("353"));
    const auto& names = ctx.first_with_command("353");
    REQUIRE(names.prefix == "pvpgn.test");
    REQUIRE(names.params[2] == "#pvpgn");
}

TEST_CASE("IrcFsm R308: NAMES in channel returns 366 RPL_ENDOFNAMES",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");

    REQUIRE(f.handle(msg("NAMES", {"#pvpgn"})).has_value());

    REQUIRE(ctx.has_command("366"));
    REQUIRE(ctx.sent.size() == 2u);
    REQUIRE(ctx.sent[0].command == "353");
    REQUIRE(ctx.sent[1].command == "366");
}

TEST_CASE("IrcFsm R308: NAMES not in channel returns only 366",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    register_user(f, ctx);

    REQUIRE(f.handle(msg("NAMES", {"#pvpgn"})).has_value());

    REQUIRE(ctx.has_command("366"));
    REQUIRE(!ctx.has_command("353"));
    REQUIRE(ctx.sent.size() == 1u);
}

TEST_CASE("IrcFsm R308: NAMES 353 params have correct structure",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");

    REQUIRE(f.handle(msg("NAMES", {"#pvpgn"})).has_value());

    const auto& names = ctx.first_with_command("353");
    // 353 params: <nick> = <channel> <nick-list>
    REQUIRE(names.params.size() >= 3u);
    REQUIRE(names.params[0] == "alice");  // target nick
    REQUIRE(names.params[1] == "=");      // channel type
    REQUIRE(names.params[2] == "#pvpgn"); // channel name
}

TEST_CASE("IrcFsm R308: NAMES 366 contains channel name",
          "[protocol][irc][fsm][channel][R308]") {
    FakeIrcCtx ctx;
    IrcFsm f{ctx};
    join_channel(f, ctx, "#pvpgn");

    REQUIRE(f.handle(msg("NAMES", {"#pvpgn"})).has_value());

    const auto& endofnames = ctx.first_with_command("366");
    REQUIRE(endofnames.prefix == "pvpgn.test");
    // make_numeric(366, target_chan, text): params[0]=target_chan, params[1]=text
    REQUIRE(endofnames.params[0] == "#pvpgn");
}
