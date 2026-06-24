// SPDX-License-Identifier: GPL-2.0-or-later
//
// WolFsm Channel Operation Tests
//
// Covers:
//   - LIST command: client sends LIST, server responds with 321/322/323 numerics
//   - JOIN #channel: client joins, server sends JOIN echo + 353 NAMES + 366
//   - PRIVMSG #channel :message: client sends message, server accepts (no error)
//   - PRIVMSG to nick: client sends private message, server responds with 401
//   - JOIN non-existent channel: server responds with 403 (stub: channel created)
//   - LIST with channels: 321 + 322 entries + 323
//   - PRIVMSG before auth: 451 ERR_NOTREGISTERED
//   - PRIVMSG with no params: 411 ERR_NORECIPIENT

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "protocol/wol/wol_fsm.hpp"
#include "protocol/wol/wol_session_context.hpp"

using namespace pvpgn;
using namespace pvpgn::protocol::wol;

// ===========================================================================
// Test helpers
// ===========================================================================

namespace {

/// Fake session context that records every line sent and whether close() was called.
class FakeWolCtx : public IWolSessionContext {
public:
    std::vector<std::string> lines;
    bool                     closed = false;

    core::Status<> send_line(std::string_view line) override {
        lines.emplace_back(line);
        return core::ok();
    }

    core::Status<> send_bytes(std::span<const std::byte> bytes) override {
        std::string buf(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        std::size_t pos = 0;
        while (pos < buf.size()) {
            auto crlf = buf.find("\r\n", pos);
            if (crlf == std::string::npos) {
                lines.push_back(buf.substr(pos));
                break;
            }
            lines.push_back(buf.substr(pos, crlf - pos));
            pos = crlf + 2;
        }
        return core::ok();
    }

    void close() override { closed = true; }

    std::string_view server_name() const noexcept override {
        return "wol.pvpgn.test";
    }

    [[nodiscard]] bool has_line_containing(std::string_view needle) const {
        for (const auto& l : lines)
            if (l.find(needle) != std::string::npos) return true;
        return false;
    }

    [[nodiscard]] std::string first_line_containing(std::string_view needle) const {
        for (const auto& l : lines)
            if (l.find(needle) != std::string::npos) return l;
        return {};
    }

    [[nodiscard]] std::size_t count_lines_containing(std::string_view needle) const {
        std::size_t n = 0;
        for (const auto& l : lines)
            if (l.find(needle) != std::string::npos) ++n;
        return n;
    }
};

/// Convert a string to a span<const byte> for feeding into the FSM.
std::span<const std::byte> as_bytes(std::string_view s) {
    return {reinterpret_cast<const std::byte*>(s.data()), s.size()};
}

/// Feed a complete IRC-style line (appends \r\n) into the FSM.
core::Status<> feed_line(WolFsm& fsm, std::string_view line) {
    std::string buf(line);
    buf += "\r\n";
    return fsm.on_bytes(as_bytes(buf));
}

/// Perform the full NICK + USER + PASS handshake (skeleton mode).
void do_auth(WolFsm& fsm, std::string_view nick = "TestUser") {
    REQUIRE(feed_line(fsm, std::string("NICK ") + std::string(nick)).has_value());
    REQUIRE(feed_line(fsm, "USER testuser localhost wol.pvpgn.test :Test User").has_value());
    REQUIRE(feed_line(fsm, "PASS testpass").has_value());
}

/// Authenticate and join a channel.
void do_auth_and_join(WolFsm& fsm, std::string_view nick = "TestUser",
                      std::string_view channel = "#lobby") {
    do_auth(fsm, nick);
    REQUIRE(feed_line(fsm, std::string("JOIN ") + std::string(channel)).has_value());
}

}  // namespace

// ===========================================================================
// LIST command — 321/322/323 numerics
// ===========================================================================

TEST_CASE("WolFsm R307: LIST after auth returns 321 RPL_LISTSTART",
          "[protocol][wol][fsm][channel][R307]") {
    auto ctx = std::make_shared<FakeWolCtx>();
    WolFsm fsm{ctx};
    do_auth(fsm);
    ctx->lines.clear();

    REQUIRE(feed_line(fsm, "LIST").has_value());
    REQUIRE(ctx->has_line_containing("321"));
}

TEST_CASE("WolFsm R307: LIST after auth returns 323 RPL_LISTEND",
          "[protocol][wol][fsm][channel][R307]") {
    auto ctx = std::make_shared<FakeWolCtx>();
    WolFsm fsm{ctx};
    do_auth(fsm);
    ctx->lines.clear();

    REQUIRE(feed_line(fsm, "LIST").has_value());
    REQUIRE(ctx->has_line_containing("323"));
}

TEST_CASE("WolFsm R307: LIST after auth — 321 comes before 323",
          "[protocol][wol][fsm][channel][R307]") {
    auto ctx = std::make_shared<FakeWolCtx>();
    WolFsm fsm{ctx};
    do_auth(fsm);
    ctx->lines.clear();

    REQUIRE(feed_line(fsm, "LIST").has_value());

    // Find positions of 321 and 323
    int pos_321 = -1, pos_323 = -1;
    for (std::size_t i = 0; i < ctx->lines.size(); ++i) {
        if (ctx->lines[i].find("321") != std::string::npos) pos_321 = static_cast<int>(i);
        if (ctx->lines[i].find("323") != std::string::npos) pos_323 = static_cast<int>(i);
    }
    REQUIRE(pos_321 >= 0);
    REQUIRE(pos_323 >= 0);
    REQUIRE(pos_321 < pos_323);
}

TEST_CASE("WolFsm R307: LIST after joining a channel includes 322 RPL_LIST entry",
          "[protocol][wol][fsm][channel][R307]") {
    auto ctx = std::make_shared<FakeWolCtx>();
    WolFsm fsm{ctx};
    do_auth_and_join(fsm, "TestUser", "#lobby");
    ctx->lines.clear();

    REQUIRE(feed_line(fsm, "LIST").has_value());

    // 321 + at least one 322 + 323
    REQUIRE(ctx->has_line_containing("321"));
    REQUIRE(ctx->has_line_containing("322"));
    REQUIRE(ctx->has_line_containing("323"));
}

TEST_CASE("WolFsm R307: LIST 322 entry contains channel name",
          "[protocol][wol][fsm][channel][R307]") {
    auto ctx = std::make_shared<FakeWolCtx>();
    WolFsm fsm{ctx};
    do_auth_and_join(fsm, "TestUser", "#lobby");
    ctx->lines.clear();

    REQUIRE(feed_line(fsm, "LIST").has_value());

    // The 322 line should contain the channel name
    auto line322 = ctx->first_line_containing("322");
    REQUIRE(!line322.empty());
    REQUIRE(line322.find("#lobby") != std::string::npos);
}

TEST_CASE("WolFsm R307: LIST before auth returns 451 ERR_NOTREGISTERED",
          "[protocol][wol][fsm][channel][R307]") {
    auto ctx = std::make_shared<FakeWolCtx>();
    WolFsm fsm{ctx};

    REQUIRE(feed_line(fsm, "LIST").has_value());
    REQUIRE(ctx->has_line_containing("451"));
}

// ===========================================================================
// JOIN #channel — JOIN echo + 353 NAMES + 366
// ===========================================================================

TEST_CASE("WolFsm R307: JOIN #channel sends JOIN echo",
          "[protocol][wol][fsm][channel][R307]") {
    auto ctx = std::make_shared<FakeWolCtx>();
    WolFsm fsm{ctx};
    do_auth(fsm, "Alice");
    ctx->lines.clear();

    REQUIRE(feed_line(fsm, "JOIN #arena").has_value());
    REQUIRE(ctx->has_line_containing("JOIN"));
    REQUIRE(ctx->has_line_containing("#arena"));
}

TEST_CASE("WolFsm R307: JOIN #channel transitions to InChannel state",
          "[protocol][wol][fsm][channel][R307]") {
    auto ctx = std::make_shared<FakeWolCtx>();
    WolFsm fsm{ctx};
    do_auth(fsm, "Alice");

    REQUIRE(feed_line(fsm, "JOIN #arena").has_value());
    REQUIRE(fsm.state() == WolState::InChannel);
    REQUIRE(fsm.channel() == "#arena");
}

TEST_CASE("WolFsm R307: JOIN #channel sends 366 RPL_ENDOFNAMES",
          "[protocol][wol][fsm][channel][R307]") {
    auto ctx = std::make_shared<FakeWolCtx>();
    WolFsm fsm{ctx};
    do_auth(fsm, "Alice");
    ctx->lines.clear();

    REQUIRE(feed_line(fsm, "JOIN #arena").has_value());
    REQUIRE(ctx->has_line_containing("366"));
}

TEST_CASE("WolFsm R307: JOIN #channel sends 353 RPL_NAMREPLY",
          "[protocol][wol][fsm][channel][R307]") {
    auto ctx = std::make_shared<FakeWolCtx>();
    WolFsm fsm{ctx};
    do_auth(fsm, "Alice");
    ctx->lines.clear();

    REQUIRE(feed_line(fsm, "JOIN #arena").has_value());
    REQUIRE(ctx->has_line_containing("353"));
}

TEST_CASE("WolFsm R307: JOIN echo contains nick prefix",
          "[protocol][wol][fsm][channel][R307]") {
    auto ctx = std::make_shared<FakeWolCtx>();
    WolFsm fsm{ctx};
    do_auth(fsm, "Alice");
    ctx->lines.clear();

    REQUIRE(feed_line(fsm, "JOIN #arena").has_value());

    // JOIN echo should contain the nick
    auto join_line = ctx->first_line_containing("JOIN");
    REQUIRE(!join_line.empty());
    REQUIRE(join_line.find("Alice") != std::string::npos);
}

TEST_CASE("WolFsm R307: JOIN before auth returns 451 ERR_NOTREGISTERED",
          "[protocol][wol][fsm][channel][R307]") {
    auto ctx = std::make_shared<FakeWolCtx>();
    WolFsm fsm{ctx};

    REQUIRE(feed_line(fsm, "JOIN #lobby").has_value());
    REQUIRE(ctx->has_line_containing("451"));
}

TEST_CASE("WolFsm R307: JOIN with no channel param returns 461 ERR_NEEDMOREPARAMS",
          "[protocol][wol][fsm][channel][R307]") {
    auto ctx = std::make_shared<FakeWolCtx>();
    WolFsm fsm{ctx};
    do_auth(fsm);
    ctx->lines.clear();

    REQUIRE(feed_line(fsm, "JOIN").has_value());
    REQUIRE(ctx->has_line_containing("461"));
}

// ===========================================================================
// PRIVMSG #channel :message — accepted (no error)
// ===========================================================================

TEST_CASE("WolFsm R307: PRIVMSG to channel after JOIN is accepted without error",
          "[protocol][wol][fsm][channel][R307]") {
    auto ctx = std::make_shared<FakeWolCtx>();
    WolFsm fsm{ctx};
    do_auth_and_join(fsm, "Bob", "#lobby");
    ctx->lines.clear();

    REQUIRE(feed_line(fsm, "PRIVMSG #lobby :hello world").has_value());

    // No 451 (not registered), no 403 (no such channel), no 404 (cannot send)
    REQUIRE(!ctx->has_line_containing("451"));
    REQUIRE(!ctx->has_line_containing("403"));
    REQUIRE(!ctx->has_line_containing("404"));
    REQUIRE(!ctx->closed);
}

TEST_CASE("WolFsm R307: PRIVMSG to channel before auth returns 451",
          "[protocol][wol][fsm][channel][R307]") {
    auto ctx = std::make_shared<FakeWolCtx>();
    WolFsm fsm{ctx};

    REQUIRE(feed_line(fsm, "PRIVMSG #lobby :hello").has_value());
    REQUIRE(ctx->has_line_containing("451"));
}

TEST_CASE("WolFsm R307: PRIVMSG to channel after auth but before JOIN is accepted",
          "[protocol][wol][fsm][channel][R307]") {
    auto ctx = std::make_shared<FakeWolCtx>();
    WolFsm fsm{ctx};
    do_auth(fsm, "Bob");
    ctx->lines.clear();

    // In Authenticated state (not InChannel), PRIVMSG to channel
    REQUIRE(feed_line(fsm, "PRIVMSG #lobby :hello").has_value());
    // Should not return 451 (already authenticated)
    REQUIRE(!ctx->has_line_containing("451"));
}

// ===========================================================================
// PRIVMSG to nick — server responds with 401 ERR_NOSUCHNICK
// ===========================================================================

TEST_CASE("WolFsm R307: PRIVMSG to nick (not a channel) returns 401 ERR_NOSUCHNICK",
          "[protocol][wol][fsm][channel][R307]") {
    auto ctx = std::make_shared<FakeWolCtx>();
    WolFsm fsm{ctx};
    do_auth_and_join(fsm, "Bob", "#lobby");
    ctx->lines.clear();

    // Private message to a nick (no '#' prefix)
    REQUIRE(feed_line(fsm, "PRIVMSG Alice :hey there").has_value());
    REQUIRE(ctx->has_line_containing("401"));
}

TEST_CASE("WolFsm R307: PRIVMSG to nick contains target nick in 401 reply",
          "[protocol][wol][fsm][channel][R307]") {
    auto ctx = std::make_shared<FakeWolCtx>();
    WolFsm fsm{ctx};
    do_auth_and_join(fsm, "Bob", "#lobby");
    ctx->lines.clear();

    REQUIRE(feed_line(fsm, "PRIVMSG Alice :hey there").has_value());

    auto line401 = ctx->first_line_containing("401");
    REQUIRE(!line401.empty());
    REQUIRE(line401.find("Alice") != std::string::npos);
}

TEST_CASE("WolFsm R307: PRIVMSG to nick in Authenticated state returns 401",
          "[protocol][wol][fsm][channel][R307]") {
    auto ctx = std::make_shared<FakeWolCtx>();
    WolFsm fsm{ctx};
    do_auth(fsm, "Bob");
    ctx->lines.clear();

    REQUIRE(feed_line(fsm, "PRIVMSG SomeNick :hello").has_value());
    REQUIRE(ctx->has_line_containing("401"));
}

// ===========================================================================
// JOIN non-existent channel — stub creates it (no 403 in skeleton mode)
// ===========================================================================

TEST_CASE("WolFsm R307: JOIN any channel name is accepted in skeleton mode",
          "[protocol][wol][fsm][channel][R307]") {
    auto ctx = std::make_shared<FakeWolCtx>();
    WolFsm fsm{ctx};
    do_auth(fsm, "Carol");
    ctx->lines.clear();

    // In skeleton mode, any channel name is accepted (no channel repository)
    REQUIRE(feed_line(fsm, "JOIN #nonexistent_channel_xyz").has_value());

    // Should get JOIN echo and 366, not 403
    REQUIRE(ctx->has_line_containing("JOIN"));
    REQUIRE(ctx->has_line_containing("366"));
    REQUIRE(!ctx->has_line_containing("403"));
}

TEST_CASE("WolFsm R307: JOIN channel name is stored in FSM state",
          "[protocol][wol][fsm][channel][R307]") {
    auto ctx = std::make_shared<FakeWolCtx>();
    WolFsm fsm{ctx};
    do_auth(fsm, "Carol");

    REQUIRE(feed_line(fsm, "JOIN #myroom").has_value());
    REQUIRE(fsm.channel() == "#myroom");
}

// ===========================================================================
// PART after JOIN
// ===========================================================================

TEST_CASE("WolFsm R307: PART after JOIN leaves channel",
          "[protocol][wol][fsm][channel][R307]") {
    auto ctx = std::make_shared<FakeWolCtx>();
    WolFsm fsm{ctx};
    do_auth_and_join(fsm, "Dave", "#lobby");
    ctx->lines.clear();

    REQUIRE(feed_line(fsm, "PART #lobby").has_value());
    REQUIRE(fsm.state() == WolState::Authenticated);
    REQUIRE(fsm.channel().empty());
    REQUIRE(ctx->has_line_containing("PART"));
}

TEST_CASE("WolFsm R307: PART when not in channel returns 442",
          "[protocol][wol][fsm][channel][R307]") {
    auto ctx = std::make_shared<FakeWolCtx>();
    WolFsm fsm{ctx};
    do_auth(fsm, "Dave");
    ctx->lines.clear();

    REQUIRE(feed_line(fsm, "PART #lobby").has_value());
    REQUIRE(ctx->has_line_containing("442"));
}
