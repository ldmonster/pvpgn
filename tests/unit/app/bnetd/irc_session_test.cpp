// SPDX-License-Identifier: GPL-2.0-or-later

/// @file irc_session_test.cpp
/// Unit tests for the IRC session wiring layer:
///   - IrcSessionFactory (callable that creates IrcTcpSession instances)
///   - IrcTcpSession implements ISessionContext (send/server_name/close)
///   - IrcFsm state transitions via a FakeIrcContext
///   - IrcEgressContext: send() encodes Message → CRLF wire bytes
///
/// These tests do NOT require a live Asio io_context. They use a
/// `FakeIrcContext` (ISessionContext stub) to exercise the FSM
/// without network I/O, and verify the IrcSessionFactory API.

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "app/bnetd/irc_session_factory.hpp"
#include "app/bnetd/irc_tcp_session.hpp"
#include "core/bytes.hpp"
#include "core/result.hpp"
#include "protocol/irc/codec.hpp"
#include "protocol/irc/fsm.hpp"
#include "protocol/irc/message.hpp"
#include "protocol/irc/session_context.hpp"

using namespace pvpgn;
using namespace pvpgn::app::bnetd;
using namespace pvpgn::protocol::irc;

// ---------------------------------------------------------------------------
// Test doubles
// ---------------------------------------------------------------------------

namespace {

/// Fake ISessionContext — records send() and close() calls.
class FakeIrcContext final : public ISessionContext {
public:
    std::vector<Message> sent_messages;
    int                  close_count  = 0;
    bool                 fail_on_send = false;
    std::string          srv_name     = "test.server";

    core::Status<> send(const Message& msg) override {
        if (fail_on_send) {
            return core::fail(core::Error{core::StatusCode::Internal,
                                          "injected send failure"});
        }
        sent_messages.push_back(msg);
        return core::ok();
    }

    std::string_view server_name() const noexcept override {
        return srv_name;
    }

    void close() override { ++close_count; }
};

}  // namespace

// ===========================================================================
// TEST SUITE 1: IrcSessionFactory API
// ===========================================================================

TEST_CASE("IrcSessionFactory: stores server_name correctly",
          "[irc_session][factory]") {
    IrcSessionFactory factory{"pvpgn.server"};
    CHECK(factory.server_name() == "pvpgn.server");
}

TEST_CASE("IrcSessionFactory: stores custom server_name",
          "[irc_session][factory]") {
    IrcSessionFactory factory{"battle.net"};
    CHECK(factory.server_name() == "battle.net");
}

TEST_CASE("IrcSessionFactory: null tcp pointer is handled gracefully",
          "[irc_session][factory]") {
    IrcSessionFactory factory{"pvpgn.server"};
    // Passing nullptr should not crash — factory guards against it
    REQUIRE_NOTHROW(factory(nullptr));
}

TEST_CASE("IrcSessionFactory: is copyable",
          "[irc_session][factory]") {
    IrcSessionFactory original{"pvpgn.server"};
    IrcSessionFactory copy = original;  // copy constructor
    CHECK(copy.server_name() == "pvpgn.server");

    IrcSessionFactory assigned{"other.server"};
    assigned = original;  // copy assignment
    CHECK(assigned.server_name() == "pvpgn.server");
}

// ===========================================================================
// TEST SUITE 2: IrcFsm state transitions via FakeIrcContext
// ===========================================================================

TEST_CASE("IrcFsm: initial state is Greeting",
          "[irc_session][fsm_state]") {
    FakeIrcContext ctx;
    IrcFsm fsm{ctx};

    CHECK(fsm.state() == IrcState::Greeting);
}

TEST_CASE("IrcFsm: NICK + USER completes registration → Registered",
          "[irc_session][fsm_state]") {
    FakeIrcContext ctx;
    IrcFsm fsm{ctx};

    REQUIRE(fsm.state() == IrcState::Greeting);

    // Send NICK
    Message nick_msg;
    nick_msg.command = "NICK";
    nick_msg.params  = {"testnick"};
    (void)fsm.handle(nick_msg);

    // Still Greeting — need USER too
    CHECK(fsm.state() == IrcState::Greeting);

    // Send USER
    Message user_msg;
    user_msg.command = "USER";
    user_msg.params  = {"testuser", "0", "*", "Test User"};
    (void)fsm.handle(user_msg);

    // Now Registered — 001 RPL_WELCOME should have been sent
    CHECK(fsm.state() == IrcState::Registered);
    CHECK(fsm.nick() == "testnick");
    CHECK(fsm.user() == "testuser");
    // At least one message sent (001 RPL_WELCOME)
    CHECK(!ctx.sent_messages.empty());
}

TEST_CASE("IrcFsm: PING in Greeting state is answered with PONG",
          "[irc_session][fsm_state]") {
    FakeIrcContext ctx;
    IrcFsm fsm{ctx};

    Message ping_msg;
    ping_msg.command = "PING";
    ping_msg.params  = {"token123"};
    (void)fsm.handle(ping_msg);

    // PONG should have been sent
    REQUIRE(!ctx.sent_messages.empty());
    const auto& pong = ctx.sent_messages.back();
    CHECK(pong.command == "PONG");
    CHECK(!pong.params.empty());
    CHECK(pong.params.back() == "token123");
}

TEST_CASE("IrcFsm: QUIT transitions to Closing and calls close()",
          "[irc_session][fsm_state]") {
    FakeIrcContext ctx;
    IrcFsm fsm{ctx};

    Message quit_msg;
    quit_msg.command = "QUIT";
    quit_msg.params  = {"Goodbye"};
    (void)fsm.handle(quit_msg);

    CHECK(fsm.state() == IrcState::Closing);
    CHECK(ctx.close_count >= 1);
}

TEST_CASE("IrcFsm: JOIN after registration transitions to InChannel",
          "[irc_session][fsm_state]") {
    FakeIrcContext ctx;
    IrcFsm fsm{ctx};

    // Register first
    Message nick_msg;
    nick_msg.command = "NICK";
    nick_msg.params  = {"joiner"};
    (void)fsm.handle(nick_msg);

    Message user_msg;
    user_msg.command = "USER";
    user_msg.params  = {"joiner", "0", "*", "Joiner"};
    (void)fsm.handle(user_msg);

    REQUIRE(fsm.state() == IrcState::Registered);
    ctx.sent_messages.clear();

    // JOIN a channel
    Message join_msg;
    join_msg.command = "JOIN";
    join_msg.params  = {"#pvpgn"};
    (void)fsm.handle(join_msg);

    CHECK(fsm.state() == IrcState::InChannel);
    CHECK(fsm.channel() == "#pvpgn");
    // JOIN echo + 366 RPL_ENDOFNAMES should have been sent
    CHECK(!ctx.sent_messages.empty());
}

TEST_CASE("IrcFsm: ERR_NOTREGISTERED for commands before registration",
          "[irc_session][fsm_state]") {
    FakeIrcContext ctx;
    IrcFsm fsm{ctx};

    // Try JOIN without registering first
    Message join_msg;
    join_msg.command = "JOIN";
    join_msg.params  = {"#pvpgn"};
    (void)fsm.handle(join_msg);

    // Should still be in Greeting
    CHECK(fsm.state() == IrcState::Greeting);
    // 451 ERR_NOTREGISTERED should have been sent
    REQUIRE(!ctx.sent_messages.empty());
    bool found_451 = false;
    for (const auto& m : ctx.sent_messages) {
        if (m.command == "451") { found_451 = true; break; }
    }
    CHECK(found_451);
}

// ===========================================================================
// TEST SUITE 3: IRC codec encode/decode round-trip
// ===========================================================================

TEST_CASE("IRC codec: encode_to_string appends CRLF",
          "[irc_session][codec]") {
    Message msg;
    msg.prefix  = "pvpgn.server";
    msg.command = "001";
    msg.params  = {"testnick", "Welcome to pvpgn.server"};

    const std::string wire = encode_to_string(msg);

    // Must end with \r\n
    REQUIRE(wire.size() >= 2);
    CHECK(wire[wire.size() - 2] == '\r');
    CHECK(wire[wire.size() - 1] == '\n');
}

TEST_CASE("IRC codec: decode round-trip for NICK command",
          "[irc_session][codec]") {
    const std::string_view line = "NICK testnick";
    auto result = decode(line);
    REQUIRE(result.has_value());
    CHECK(result.value().command == "NICK");
    REQUIRE(result.value().params.size() == 1);
    CHECK(result.value().params[0] == "testnick");
}

TEST_CASE("IRC codec: try_parse_line extracts CRLF-terminated line",
          "[irc_session][codec]") {
    const std::string buf = "NICK foo\r\nUSER bar 0 * :Bar\r\n";
    auto result = try_parse_line(buf);
    REQUIRE(result.has_value());
    CHECK(result.value().line == "NICK foo");
    CHECK(result.value().consumed == 10);  // "NICK foo\r\n" = 10 bytes
}

TEST_CASE("IRC codec: try_parse_line returns NeedMore for incomplete line",
          "[irc_session][codec]") {
    const std::string buf = "NICK foo";  // no CRLF
    auto result = try_parse_line(buf);
    CHECK(!result.has_value());
}

// ===========================================================================
// TEST SUITE 4: IrcTcpSession line-buffer cap (remote-DoS regression)
// ===========================================================================
//
// An IrcTcpSession is constructed with a null TcpSession: on_bytes() drives
// the framing/accumulation path without needing a live Asio transport, and
// close() is a no-op guard on a null tcp_ (so the cap path is observable via
// the return value and rx_buf_size()).

TEST_CASE("IrcTcpSession: unterminated flood past cap closes the session",
          "[irc_session][dos]") {
    auto session =
        std::make_shared<IrcTcpSession>(nullptr, std::string{"pvpgn.server"});

    // Stream printable bytes with no CRLF, far exceeding the cap.
    const std::string flood(kIrcMaxLineLen * 4, 'A');
    const bool alive = session->on_bytes(core::as_byte_view(flood));

    // The runaway client is rejected and the buffer reset — not grown.
    CHECK(alive == false);
    CHECK(session->rx_buf_size() == 0);
}

TEST_CASE("IrcTcpSession: rx_buf never grows past the cap across many chunks",
          "[irc_session][dos]") {
    auto session =
        std::make_shared<IrcTcpSession>(nullptr, std::string{"pvpgn.server"});

    const std::string chunk(256, 'B');  // no CRLF
    bool alive = true;
    std::size_t fed = 0;
    while (alive && fed < kIrcMaxLineLen * 8) {
        alive = session->on_bytes(core::as_byte_view(chunk));
        fed += chunk.size();
        // Until it trips the cap, the buffer must stay bounded by it.
        CHECK(session->rx_buf_size() <= kIrcMaxLineLen);
    }
    // It must have tripped the cap rather than looping forever.
    CHECK(alive == false);
}

TEST_CASE("IrcTcpSession: normal CRLF-terminated line still parses after cap",
          "[irc_session][dos]") {
    auto session =
        std::make_shared<IrcTcpSession>(nullptr, std::string{"pvpgn.server"});

    // A well-formed NICK line is consumed entirely — buffer drains to empty
    // and the session stays alive.
    const bool alive = session->on_bytes(core::as_byte_view("NICK testnick\r\n"));
    CHECK(alive == true);
    CHECK(session->rx_buf_size() == 0);
}

TEST_CASE("IrcTcpSession: long-but-terminated line under cap is accepted",
          "[irc_session][dos]") {
    auto session =
        std::make_shared<IrcTcpSession>(nullptr, std::string{"pvpgn.server"});

    // A line just under the cap, properly terminated, parses without tripping
    // the guard (decode may reject it as malformed, but it must not close).
    std::string line(kIrcMaxLineLen - 2, 'x');
    line += "\r\n";
    const bool alive = session->on_bytes(core::as_byte_view(line));
    CHECK(alive == true);
    CHECK(session->rx_buf_size() == 0);
}
