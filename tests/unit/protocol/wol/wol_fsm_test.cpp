// SPDX-License-Identifier: GPL-2.0-or-later
//
// Unit tests for WolFsm — WOL (Westwood Online) IRC-like chat protocol FSM.
//
// Tests cover:
//   - Initial state is Connecting
//   - NICK command transitions to Authenticating
//   - NICK + USER + PASS sequence → Authenticated state + 001 welcome
//   - PING token → PONG token response
//   - QUIT → Disconnecting state
//   - Partial line buffering (bytes split across on_bytes calls)
//   - Multiple commands in one on_bytes call
//   - LIST command returns empty channel list (321 + 323)
//   - JOIN #channel → join confirmation (JOIN echo + 366)
//   - PART → leave channel
//   - PRIVMSG before auth → 451 ERR_NOTREGISTERED
//   - Unknown command → 421 ERR_UNKNOWNCOMMAND
//   - NICK with no nickname → 431 ERR_NONICKNAMEGIVEN
//   - USER with too few params → 461 ERR_NEEDMOREPARAMS
//   - on_close() transitions to Disconnecting

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
class FakeWolContext : public IWolSessionContext {
public:
    std::vector<std::string> lines;  ///< Each \r\n-terminated line, stripped of \r\n
    bool                     closed = false;

    core::Status<> send_line(std::string_view line) override {
        lines.emplace_back(line);
        return core::ok();
    }

    core::Status<> send_bytes(std::span<const std::byte> bytes) override {
        // Split on \r\n and accumulate lines.
        std::string buf(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        std::size_t pos = 0;
        while (pos < buf.size()) {
            auto crlf = buf.find("\r\n", pos);
            if (crlf == std::string::npos) {
                // Partial line — store as-is (shouldn't happen in tests).
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

    /// Return true if any recorded line contains the given substring.
    bool has_line_containing(std::string_view needle) const {
        for (const auto& l : lines) {
            if (l.find(needle) != std::string::npos) return true;
        }
        return false;
    }

    /// Return the first line containing needle, or empty string.
    std::string first_line_containing(std::string_view needle) const {
        for (const auto& l : lines) {
            if (l.find(needle) != std::string::npos) return l;
        }
        return {};
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

/// Perform the full NICK + USER + PASS handshake.
void do_auth(WolFsm& fsm, std::string_view nick = "TestUser") {
    REQUIRE(feed_line(fsm, std::string("NICK ") + std::string(nick)).has_value());
    REQUIRE(feed_line(fsm, "USER testuser localhost wol.pvpgn.test :Test User").has_value());
    REQUIRE(feed_line(fsm, "PASS testpass").has_value());
}

}  // namespace

// ===========================================================================
// Tests
// ===========================================================================

// ---------------------------------------------------------------------------
// Initial state
// ---------------------------------------------------------------------------

TEST_CASE("WolFsm: initial state is Connecting",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};
    REQUIRE(fsm.state() == WolState::Connecting);
    REQUIRE(fsm.nick().empty());
    REQUIRE(fsm.channel().empty());
}

// ---------------------------------------------------------------------------
// NICK command
// ---------------------------------------------------------------------------

TEST_CASE("WolFsm: NICK transitions to Authenticating",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};

    REQUIRE(feed_line(fsm, "NICK Alice").has_value());
    REQUIRE(fsm.state() == WolState::Authenticating);
    REQUIRE(fsm.nick() == "Alice");
    REQUIRE(ctx->lines.empty());  // no reply yet
}

TEST_CASE("WolFsm: NICK with no nickname emits 431",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};

    REQUIRE(feed_line(fsm, "NICK").has_value());
    REQUIRE(ctx->has_line_containing("431"));
}

TEST_CASE("WolFsm: NICK with empty param emits 431",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};

    REQUIRE(feed_line(fsm, "NICK ").has_value());
    REQUIRE(ctx->has_line_containing("431"));
}

// ---------------------------------------------------------------------------
// Full authentication sequence
// ---------------------------------------------------------------------------

TEST_CASE("WolFsm: NICK + USER + PASS → Authenticated + 001 welcome",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};

    REQUIRE(feed_line(fsm, "NICK Bob").has_value());
    REQUIRE(fsm.state() == WolState::Authenticating);

    REQUIRE(feed_line(fsm, "USER bob localhost wol.pvpgn.test :Bob Smith").has_value());
    // Still Authenticating — PASS not yet received.
    REQUIRE(fsm.state() == WolState::Authenticating);

    REQUIRE(feed_line(fsm, "PASS secret").has_value());
    REQUIRE(fsm.state() == WolState::Authenticated);

    // Must have sent 001 RPL_WELCOME
    REQUIRE(ctx->has_line_containing("001"));
    REQUIRE(ctx->has_line_containing("Bob"));
    REQUIRE(ctx->has_line_containing("Welcome"));
}

TEST_CASE("WolFsm: 001 welcome contains server name",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};

    do_auth(fsm, "Charlie");

    auto welcome = ctx->first_line_containing("001");
    REQUIRE(!welcome.empty());
    REQUIRE(welcome.find("wol.pvpgn.test") != std::string::npos);
}

TEST_CASE("WolFsm: auth sends 002 RPL_YOURHOST",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};
    do_auth(fsm);
    REQUIRE(ctx->has_line_containing("002"));
}

TEST_CASE("WolFsm: auth sends 376 RPL_ENDOFMOTD",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};
    do_auth(fsm);
    REQUIRE(ctx->has_line_containing("376"));
}

// ---------------------------------------------------------------------------
// PING / PONG
// ---------------------------------------------------------------------------

TEST_CASE("WolFsm: PING token → PONG token",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};

    REQUIRE(feed_line(fsm, "PING :12345").has_value());
    REQUIRE(ctx->has_line_containing("PONG"));
    REQUIRE(ctx->has_line_containing("12345"));
}

TEST_CASE("WolFsm: PING without colon prefix also works",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};

    REQUIRE(feed_line(fsm, "PING abctoken").has_value());
    REQUIRE(ctx->has_line_containing("PONG"));
    REQUIRE(ctx->has_line_containing("abctoken"));
}

TEST_CASE("WolFsm: PING works in any state",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};

    // Before auth
    REQUIRE(feed_line(fsm, "PING tok1").has_value());
    REQUIRE(ctx->has_line_containing("tok1"));

    ctx->lines.clear();
    do_auth(fsm);
    ctx->lines.clear();

    // After auth
    REQUIRE(feed_line(fsm, "PING tok2").has_value());
    REQUIRE(ctx->has_line_containing("tok2"));
}

// ---------------------------------------------------------------------------
// QUIT
// ---------------------------------------------------------------------------

TEST_CASE("WolFsm: QUIT → Disconnecting state",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};

    REQUIRE(feed_line(fsm, "QUIT :Goodbye").has_value());
    REQUIRE(fsm.state() == WolState::Disconnecting);
    REQUIRE(ctx->closed);
}

TEST_CASE("WolFsm: QUIT sends RPL_QUIT (607) goodbye line",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};

    // The original _handle_quit_command replies with numeric 607 ":goodbye"
    // (":<server> 607 <nick> :goodbye"); unauthenticated, the nick is "*".
    REQUIRE(feed_line(fsm, "QUIT").has_value());
    REQUIRE(ctx->has_line_containing(" 607 "));
    REQUIRE(ctx->has_line_containing(":goodbye"));
}

TEST_CASE("WolFsm: bytes after QUIT are ignored",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};

    REQUIRE(feed_line(fsm, "QUIT").has_value());
    ctx->lines.clear();

    // Feed more bytes — should be silently ignored.
    REQUIRE(feed_line(fsm, "NICK ghost").has_value());
    REQUIRE(ctx->lines.empty());
    REQUIRE(fsm.state() == WolState::Disconnecting);
}

// ---------------------------------------------------------------------------
// on_close()
// ---------------------------------------------------------------------------

TEST_CASE("WolFsm: on_close transitions to Disconnecting",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};

    fsm.on_close();
    REQUIRE(fsm.state() == WolState::Disconnecting);
}

// ---------------------------------------------------------------------------
// Partial line buffering
// ---------------------------------------------------------------------------

TEST_CASE("WolFsm: partial line is buffered without dispatch",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};

    // Feed "NICK Al" without \r\n — no dispatch yet.
    REQUIRE(fsm.on_bytes(as_bytes("NICK Al")).has_value());
    REQUIRE(fsm.state() == WolState::Connecting);
    REQUIRE(ctx->lines.empty());

    // Feed "ice\r\n" — now the line is complete.
    REQUIRE(fsm.on_bytes(as_bytes("ice\r\n")).has_value());
    REQUIRE(fsm.state() == WolState::Authenticating);
    REQUIRE(fsm.nick() == "Alice");
}

TEST_CASE("WolFsm: line split across many on_bytes calls",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};

    // Feed one byte at a time.
    std::string line = "PING :xyz\r\n";
    for (char c : line) {
        REQUIRE(fsm.on_bytes(as_bytes(std::string_view{&c, 1})).has_value());
    }
    REQUIRE(ctx->has_line_containing("PONG"));
    REQUIRE(ctx->has_line_containing("xyz"));
}

// ---------------------------------------------------------------------------
// Multiple commands in one on_bytes call
// ---------------------------------------------------------------------------

TEST_CASE("WolFsm: multiple commands in one on_bytes call",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};

    // Send NICK + USER + PASS in one chunk.
    std::string chunk =
        "NICK Dave\r\n"
        "USER dave localhost wol.pvpgn.test :Dave\r\n"
        "PASS pw\r\n";

    REQUIRE(fsm.on_bytes(as_bytes(chunk)).has_value());
    REQUIRE(fsm.state() == WolState::Authenticated);
    REQUIRE(ctx->has_line_containing("001"));
}

TEST_CASE("WolFsm: two PINGs in one on_bytes call",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};

    std::string chunk = "PING :aaa\r\nPING :bbb\r\n";
    REQUIRE(fsm.on_bytes(as_bytes(chunk)).has_value());

    REQUIRE(ctx->has_line_containing("aaa"));
    REQUIRE(ctx->has_line_containing("bbb"));
}

// ---------------------------------------------------------------------------
// LIST command
// ---------------------------------------------------------------------------

TEST_CASE("WolFsm: LIST before auth → 451 not registered",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};

    REQUIRE(feed_line(fsm, "LIST").has_value());
    REQUIRE(ctx->has_line_containing("451"));
}

TEST_CASE("WolFsm: LIST after auth returns empty channel list",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};
    do_auth(fsm);
    ctx->lines.clear();

    REQUIRE(feed_line(fsm, "LIST").has_value());
    // 321 RPL_LISTSTART
    REQUIRE(ctx->has_line_containing("321"));
    // 323 RPL_LISTEND
    REQUIRE(ctx->has_line_containing("323"));
}

// ---------------------------------------------------------------------------
// JOIN command
// ---------------------------------------------------------------------------

TEST_CASE("WolFsm: JOIN before auth → 451 not registered",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};

    REQUIRE(feed_line(fsm, "JOIN #lobby").has_value());
    REQUIRE(ctx->has_line_containing("451"));
}

TEST_CASE("WolFsm: JOIN #channel → join confirmation",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};
    do_auth(fsm, "Eve");
    ctx->lines.clear();

    REQUIRE(feed_line(fsm, "JOIN #lobby").has_value());
    REQUIRE(fsm.state() == WolState::InChannel);
    REQUIRE(fsm.channel() == "#lobby");

    // JOIN echo with user prefix
    REQUIRE(ctx->has_line_containing("JOIN"));
    REQUIRE(ctx->has_line_containing("#lobby"));
    // 366 RPL_ENDOFNAMES
    REQUIRE(ctx->has_line_containing("366"));
}

TEST_CASE("WolFsm: JOIN with no channel → 461",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};
    do_auth(fsm);
    ctx->lines.clear();

    REQUIRE(feed_line(fsm, "JOIN").has_value());
    REQUIRE(ctx->has_line_containing("461"));
}

// ---------------------------------------------------------------------------
// PART command
// ---------------------------------------------------------------------------

TEST_CASE("WolFsm: PART after JOIN → leaves channel",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};
    do_auth(fsm, "Frank");
    REQUIRE(feed_line(fsm, "JOIN #arena").has_value());
    ctx->lines.clear();

    REQUIRE(feed_line(fsm, "PART #arena").has_value());
    REQUIRE(fsm.state() == WolState::Authenticated);
    REQUIRE(fsm.channel().empty());
    REQUIRE(ctx->has_line_containing("PART"));
}

TEST_CASE("WolFsm: PART when not in channel is a silent no-op",
          "[protocol][wol][fsm]") {
    // The original ignores PART when not on a channel (conn_part_channel no-op);
    // it never replies 442. v3 matches: no reply line at all.
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};
    do_auth(fsm);
    ctx->lines.clear();

    REQUIRE(feed_line(fsm, "PART #nowhere").has_value());
    REQUIRE_FALSE(ctx->has_line_containing("442"));
    REQUIRE(ctx->lines.empty());
}

// ---------------------------------------------------------------------------
// PRIVMSG
// ---------------------------------------------------------------------------

TEST_CASE("WolFsm: PRIVMSG before auth → 451",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};

    REQUIRE(feed_line(fsm, "PRIVMSG #lobby :hello").has_value());
    REQUIRE(ctx->has_line_containing("451"));
}

TEST_CASE("WolFsm: PRIVMSG after auth is accepted (stub)",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};
    do_auth(fsm);
    REQUIRE(feed_line(fsm, "JOIN #lobby").has_value());
    ctx->lines.clear();

    // Stub: accepted without error, no relay yet.
    REQUIRE(feed_line(fsm, "PRIVMSG #lobby :hello world").has_value());
    // No 451 error
    REQUIRE(!ctx->has_line_containing("451"));
}

// ---------------------------------------------------------------------------
// Unknown command
// ---------------------------------------------------------------------------

TEST_CASE("WolFsm: unknown command → 421 ERR_UNKNOWNCOMMAND",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};

    REQUIRE(feed_line(fsm, "FOOBAR :test").has_value());
    REQUIRE(ctx->has_line_containing("421"));
    REQUIRE(ctx->has_line_containing("FOOBAR"));
}

TEST_CASE("WolFsm: unknown command after auth → 421",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};
    do_auth(fsm);
    ctx->lines.clear();

    REQUIRE(feed_line(fsm, "XYZZY").has_value());
    REQUIRE(ctx->has_line_containing("421"));
}

// ---------------------------------------------------------------------------
// USER parameter validation
// ---------------------------------------------------------------------------

TEST_CASE("WolFsm: USER with too few params → 461",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};

    REQUIRE(feed_line(fsm, "NICK Alice").has_value());
    REQUIRE(feed_line(fsm, "USER").has_value());
    REQUIRE(ctx->has_line_containing("461"));
}

// ---------------------------------------------------------------------------
// WOL-specific commands are silently accepted (no 421)
// ---------------------------------------------------------------------------

TEST_CASE("WolFsm: WOL-specific commands are silently accepted",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};
    do_auth(fsm);
    ctx->lines.clear();

    // These are WOL-specific commands that should not return 421.
    for (auto cmd : {"CVERS 0 0", "VERCHK 0 0", "APGAR test", "SETOPT 0"}) {
        ctx->lines.clear();
        REQUIRE(feed_line(fsm, cmd).has_value());
        REQUIRE(!ctx->has_line_containing("421"));
    }
}

// ---------------------------------------------------------------------------
// PONG from client is silently ignored
// ---------------------------------------------------------------------------

TEST_CASE("WolFsm: PONG from client is silently ignored",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};

    REQUIRE(feed_line(fsm, "PONG :server").has_value());
    REQUIRE(ctx->lines.empty());
}

// ---------------------------------------------------------------------------
// Bare \n line terminator (lenient parsing)
// ---------------------------------------------------------------------------

TEST_CASE("WolFsm: bare LF line terminator is accepted",
          "[protocol][wol][fsm]") {
    auto ctx = std::make_shared<FakeWolContext>();
    WolFsm fsm{ctx};

    // Feed with bare \n instead of \r\n.
    REQUIRE(fsm.on_bytes(as_bytes("NICK Grace\n")).has_value());
    REQUIRE(fsm.state() == WolState::Authenticating);
    REQUIRE(fsm.nick() == "Grace");
}
