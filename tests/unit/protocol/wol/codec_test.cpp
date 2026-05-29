// SPDX-License-Identifier: GPL-2.0-or-later
//
// Unit tests for the WOL (Westwood Online) codec.
//
// Coverage:
//   - NICK round-trip (encode NumericReply → decode_client NICK)
//   - USER decode
//   - PASS decode
//   - PING decode
//   - PONG decode
//   - QUIT decode
//   - LIST decode
//   - JOIN decode
//   - PART decode
//   - PRIVMSG decode
//   - CVERS decode
//   - VERCHK decode
//   - APGAR decode
//   - SETOPT decode
//   - SERIAL decode
//   - GAMEOPT decode
//   - STARTG decode (with player list)
//   - JOINGAME decode
//   - FINDUSER decode
//   - PAGE decode
//   - ADDBUDDY decode
//   - DELBUDDY decode
//   - GETBUDDY decode
//   - WolCommand catch-all (known WOL command)
//   - IRC prefix stripped before command
//   - Case-insensitive command matching
//   - Empty line → DecodeError::Truncated
//   - Unknown opcode → DecodeError::UnknownOpcode
//   - NICK with no nickname → DecodeError::MalformedString
//   - NumericReply encode
//   - RawLine encode
//   - ServerMessage variant encode

#include <string>
#include <variant>

#include <catch2/catch_test_macros.hpp>

#include "protocol/common/decode_error.hpp"
#include "protocol/wol/codec.hpp"
#include "protocol/wol/messages.hpp"

using namespace pvpgn;
using namespace pvpgn::protocol::wol;
using pvpgn::protocol::common::DecodeError;

// ===========================================================================
// Helpers
// ===========================================================================

namespace {

/// Decode a line and assert it succeeded, returning the ClientMessage.
ClientMessage must_decode(std::string_view line) {
    auto r = decode_client(line);
    REQUIRE(r.has_value());
    return r.value();
}

/// Decode a line and assert it failed with the given error.
void must_fail(std::string_view line, DecodeError expected) {
    auto r = decode_client(line);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == expected);
}

}  // namespace

// ===========================================================================
// Decode: core IRC commands
// ===========================================================================

TEST_CASE("wol codec: NICK decode", "[protocol][wol][codec]") {
    auto msg = must_decode("NICK PlayerOne");
    REQUIRE(std::holds_alternative<Nick>(msg));
    REQUIRE(std::get<Nick>(msg).nickname == "PlayerOne");
}

TEST_CASE("wol codec: NICK with colon prefix", "[protocol][wol][codec]") {
    auto msg = must_decode("NICK :PlayerOne");
    REQUIRE(std::holds_alternative<Nick>(msg));
    REQUIRE(std::get<Nick>(msg).nickname == "PlayerOne");
}

TEST_CASE("wol codec: USER decode", "[protocol][wol][codec]") {
    auto msg = must_decode("USER player 0 * :Player One");
    REQUIRE(std::holds_alternative<User>(msg));
    const auto& u = std::get<User>(msg);
    REQUIRE(u.username   == "player");
    REQUIRE(u.hostname   == "0");
    REQUIRE(u.servername == "*");
    REQUIRE(u.realname   == "Player One");
}

TEST_CASE("wol codec: PASS decode", "[protocol][wol][codec]") {
    auto msg = must_decode("PASS secrethash");
    REQUIRE(std::holds_alternative<Pass>(msg));
    REQUIRE(std::get<Pass>(msg).password == "secrethash");
}

TEST_CASE("wol codec: PASS with colon prefix", "[protocol][wol][codec]") {
    auto msg = must_decode("PASS :secrethash");
    REQUIRE(std::holds_alternative<Pass>(msg));
    REQUIRE(std::get<Pass>(msg).password == "secrethash");
}

TEST_CASE("wol codec: PING decode", "[protocol][wol][codec]") {
    auto msg = must_decode("PING 12345");
    REQUIRE(std::holds_alternative<Ping>(msg));
    REQUIRE(std::get<Ping>(msg).token == "12345");
}

TEST_CASE("wol codec: PONG decode", "[protocol][wol][codec]") {
    auto msg = must_decode("PONG :12345");
    REQUIRE(std::holds_alternative<Pong>(msg));
    REQUIRE(std::get<Pong>(msg).token == "12345");
}

TEST_CASE("wol codec: QUIT decode with reason", "[protocol][wol][codec]") {
    auto msg = must_decode("QUIT :Goodbye");
    REQUIRE(std::holds_alternative<Quit>(msg));
    REQUIRE(std::get<Quit>(msg).reason == "Goodbye");
}

TEST_CASE("wol codec: QUIT decode without reason", "[protocol][wol][codec]") {
    auto msg = must_decode("QUIT");
    REQUIRE(std::holds_alternative<Quit>(msg));
    REQUIRE(std::get<Quit>(msg).reason.empty());
}

TEST_CASE("wol codec: LIST decode", "[protocol][wol][codec]") {
    auto msg = must_decode("LIST");
    REQUIRE(std::holds_alternative<List>(msg));
}

TEST_CASE("wol codec: JOIN decode", "[protocol][wol][codec]") {
    auto msg = must_decode("JOIN #lobby");
    REQUIRE(std::holds_alternative<Join>(msg));
    REQUIRE(std::get<Join>(msg).channel == "#lobby");
}

TEST_CASE("wol codec: PART decode with reason", "[protocol][wol][codec]") {
    auto msg = must_decode("PART #lobby :leaving");
    REQUIRE(std::holds_alternative<Part>(msg));
    const auto& p = std::get<Part>(msg);
    REQUIRE(p.channel == "#lobby");
    REQUIRE(p.reason  == "leaving");
}

TEST_CASE("wol codec: PART decode without reason", "[protocol][wol][codec]") {
    auto msg = must_decode("PART #lobby");
    REQUIRE(std::holds_alternative<Part>(msg));
    REQUIRE(std::get<Part>(msg).channel == "#lobby");
    REQUIRE(std::get<Part>(msg).reason.empty());
}

TEST_CASE("wol codec: PRIVMSG decode", "[protocol][wol][codec]") {
    auto msg = must_decode("PRIVMSG #lobby :Hello everyone");
    REQUIRE(std::holds_alternative<Privmsg>(msg));
    const auto& pm = std::get<Privmsg>(msg);
    REQUIRE(pm.target  == "#lobby");
    REQUIRE(pm.message == "Hello everyone");
}

// ===========================================================================
// Decode: WOL-specific commands
// ===========================================================================

TEST_CASE("wol codec: CVERS decode", "[protocol][wol][codec]") {
    auto msg = must_decode("CVERS GAME 1234");
    REQUIRE(std::holds_alternative<Cvers>(msg));
    const auto& c = std::get<Cvers>(msg);
    REQUIRE(c.client_id == "GAME");
    REQUIRE(c.version   == "1234");
}

TEST_CASE("wol codec: VERCHK decode", "[protocol][wol][codec]") {
    auto msg = must_decode("VERCHK GAME 1234");
    REQUIRE(std::holds_alternative<Verchk>(msg));
    const auto& v = std::get<Verchk>(msg);
    REQUIRE(v.client_id == "GAME");
    REQUIRE(v.version   == "1234");
}

TEST_CASE("wol codec: APGAR decode", "[protocol][wol][codec]") {
    auto msg = must_decode("APGAR abc123hash 0");
    REQUIRE(std::holds_alternative<Apgar>(msg));
    const auto& a = std::get<Apgar>(msg);
    REQUIRE(a.password_hash == "abc123hash");
    REQUIRE(a.flags         == "0");
}

TEST_CASE("wol codec: SETOPT decode", "[protocol][wol][codec]") {
    auto msg = must_decode("SETOPT CODEPAGE 1252");
    REQUIRE(std::holds_alternative<Setopt>(msg));
    const auto& s = std::get<Setopt>(msg);
    REQUIRE(s.option == "CODEPAGE");
    REQUIRE(s.value  == "1252");
}

TEST_CASE("wol codec: SERIAL decode", "[protocol][wol][codec]") {
    auto msg = must_decode("SERIAL ABCD-1234-EFGH-5678");
    REQUIRE(std::holds_alternative<Serial>(msg));
    REQUIRE(std::get<Serial>(msg).serial_number == "ABCD-1234-EFGH-5678");
}

TEST_CASE("wol codec: GAMEOPT decode", "[protocol][wol][codec]") {
    auto msg = must_decode("GAMEOPT speed=6 color=red");
    REQUIRE(std::holds_alternative<Gameopt>(msg));
    REQUIRE(std::get<Gameopt>(msg).options == "speed=6 color=red");
}

TEST_CASE("wol codec: STARTG decode with players", "[protocol][wol][codec]") {
    auto msg = must_decode("STARTG MyGame Alice Bob Charlie");
    REQUIRE(std::holds_alternative<Startg>(msg));
    const auto& sg = std::get<Startg>(msg);
    REQUIRE(sg.game_name == "MyGame");
    REQUIRE(sg.players.size() == 3);
    REQUIRE(sg.players[0] == "Alice");
    REQUIRE(sg.players[1] == "Bob");
    REQUIRE(sg.players[2] == "Charlie");
}

TEST_CASE("wol codec: STARTG decode without players", "[protocol][wol][codec]") {
    auto msg = must_decode("STARTG MyGame");
    REQUIRE(std::holds_alternative<Startg>(msg));
    const auto& sg = std::get<Startg>(msg);
    REQUIRE(sg.game_name == "MyGame");
    REQUIRE(sg.players.empty());
}

TEST_CASE("wol codec: JOINGAME decode", "[protocol][wol][codec]") {
    auto msg = must_decode("JOINGAME MyGame 192.168.1.1 8080");
    REQUIRE(std::holds_alternative<Joingame>(msg));
    const auto& jg = std::get<Joingame>(msg);
    REQUIRE(jg.game_name == "MyGame");
    REQUIRE(jg.host_ip   == "192.168.1.1");
    REQUIRE(jg.host_port == "8080");
}

TEST_CASE("wol codec: FINDUSER decode", "[protocol][wol][codec]") {
    auto msg = must_decode("FINDUSER Alice");
    REQUIRE(std::holds_alternative<Finduser>(msg));
    REQUIRE(std::get<Finduser>(msg).nickname == "Alice");
}

TEST_CASE("wol codec: PAGE decode", "[protocol][wol][codec]") {
    auto msg = must_decode("PAGE Alice :Hey there!");
    REQUIRE(std::holds_alternative<Page>(msg));
    const auto& pg = std::get<Page>(msg);
    REQUIRE(pg.nickname == "Alice");
    REQUIRE(pg.message  == "Hey there!");
}

TEST_CASE("wol codec: ADDBUDDY decode", "[protocol][wol][codec]") {
    auto msg = must_decode("ADDBUDDY Alice");
    REQUIRE(std::holds_alternative<Addbuddy>(msg));
    REQUIRE(std::get<Addbuddy>(msg).nickname == "Alice");
}

TEST_CASE("wol codec: DELBUDDY decode", "[protocol][wol][codec]") {
    auto msg = must_decode("DELBUDDY Alice");
    REQUIRE(std::holds_alternative<Delbuddy>(msg));
    REQUIRE(std::get<Delbuddy>(msg).nickname == "Alice");
}

TEST_CASE("wol codec: GETBUDDY decode", "[protocol][wol][codec]") {
    auto msg = must_decode("GETBUDDY");
    REQUIRE(std::holds_alternative<Getbuddy>(msg));
}

TEST_CASE("wol codec: WolCommand catch-all for CHANCHK", "[protocol][wol][codec]") {
    auto msg = must_decode("CHANCHK #lobby");
    REQUIRE(std::holds_alternative<WolCommand>(msg));
    const auto& wc = std::get<WolCommand>(msg);
    REQUIRE(wc.command == "CHANCHK");
    REQUIRE(wc.params  == "#lobby");
}

TEST_CASE("wol codec: WolCommand catch-all for MODE", "[protocol][wol][codec]") {
    auto msg = must_decode("MODE #lobby +m");
    REQUIRE(std::holds_alternative<WolCommand>(msg));
    const auto& wc = std::get<WolCommand>(msg);
    REQUIRE(wc.command == "MODE");
    REQUIRE(wc.params  == "#lobby +m");
}

TEST_CASE("wol codec: WolCommand catch-all for USERIP", "[protocol][wol][codec]") {
    auto msg = must_decode("USERIP Alice");
    REQUIRE(std::holds_alternative<WolCommand>(msg));
    REQUIRE(std::get<WolCommand>(msg).command == "USERIP");
}

// ===========================================================================
// Decode: IRC prefix stripping
// ===========================================================================

TEST_CASE("wol codec: IRC prefix is stripped", "[protocol][wol][codec]") {
    // Lines from server-to-server or relayed clients may have ":prefix CMD ..."
    auto msg = must_decode(":server.wol.ea.com NICK PlayerOne");
    REQUIRE(std::holds_alternative<Nick>(msg));
    REQUIRE(std::get<Nick>(msg).nickname == "PlayerOne");
}

// ===========================================================================
// Decode: case-insensitive command matching
// ===========================================================================

TEST_CASE("wol codec: lowercase command is accepted", "[protocol][wol][codec]") {
    auto msg = must_decode("nick LowerPlayer");
    REQUIRE(std::holds_alternative<Nick>(msg));
    REQUIRE(std::get<Nick>(msg).nickname == "LowerPlayer");
}

TEST_CASE("wol codec: mixed-case command is accepted", "[protocol][wol][codec]") {
    auto msg = must_decode("Join #lobby");
    REQUIRE(std::holds_alternative<Join>(msg));
    REQUIRE(std::get<Join>(msg).channel == "#lobby");
}

// ===========================================================================
// Decode: error cases
// ===========================================================================

TEST_CASE("wol codec: empty line returns Truncated", "[protocol][wol][codec]") {
    must_fail("", DecodeError::Truncated);
}

TEST_CASE("wol codec: unknown opcode returns UnknownOpcode", "[protocol][wol][codec]") {
    must_fail("XYZZY something", DecodeError::UnknownOpcode);
}

TEST_CASE("wol codec: NICK with no nickname returns MalformedString",
          "[protocol][wol][codec]") {
    must_fail("NICK", DecodeError::MalformedString);
}

TEST_CASE("wol codec: NICK with only spaces returns MalformedString",
          "[protocol][wol][codec]") {
    must_fail("NICK   ", DecodeError::MalformedString);
}

TEST_CASE("wol codec: JOIN with no channel returns MalformedString",
          "[protocol][wol][codec]") {
    must_fail("JOIN", DecodeError::MalformedString);
}

TEST_CASE("wol codec: STARTG with no game name returns MalformedString",
          "[protocol][wol][codec]") {
    must_fail("STARTG", DecodeError::MalformedString);
}

TEST_CASE("wol codec: FINDUSER with no nickname returns MalformedString",
          "[protocol][wol][codec]") {
    must_fail("FINDUSER", DecodeError::MalformedString);
}

TEST_CASE("wol codec: ADDBUDDY with no nickname returns MalformedString",
          "[protocol][wol][codec]") {
    must_fail("ADDBUDDY", DecodeError::MalformedString);
}

TEST_CASE("wol codec: DELBUDDY with no nickname returns MalformedString",
          "[protocol][wol][codec]") {
    must_fail("DELBUDDY", DecodeError::MalformedString);
}

// ===========================================================================
// Encode: server messages
// ===========================================================================

TEST_CASE("wol codec: NumericReply encode — 001 welcome", "[protocol][wol][codec]") {
    NumericReply reply;
    reply.server_name = "wol.ea.com";
    reply.code        = 1;
    reply.target      = "PlayerOne";
    reply.text        = "Welcome to WOL";

    const auto wire = encode_server(reply);
    REQUIRE(wire == ":wol.ea.com 001 PlayerOne :Welcome to WOL\r\n");
}

TEST_CASE("wol codec: NumericReply encode — 421 unknown command", "[protocol][wol][codec]") {
    NumericReply reply;
    reply.server_name = "wol.ea.com";
    reply.code        = 421;
    reply.target      = "PlayerOne";
    reply.text        = "XYZZY :Unknown command";

    const auto wire = encode_server(reply);
    REQUIRE(wire == ":wol.ea.com 421 PlayerOne :XYZZY :Unknown command\r\n");
}

TEST_CASE("wol codec: RawLine encode — PONG", "[protocol][wol][codec]") {
    RawLine raw;
    raw.line = "PONG :12345";

    const auto wire = encode_server(raw);
    REQUIRE(wire == "PONG :12345\r\n");
}

TEST_CASE("wol codec: RawLine encode — JOIN echo", "[protocol][wol][codec]") {
    RawLine raw;
    raw.line = ":PlayerOne JOIN :#lobby";

    const auto wire = encode_server(raw);
    REQUIRE(wire == ":PlayerOne JOIN :#lobby\r\n");
}

TEST_CASE("wol codec: ServerMessage variant encode — NumericReply",
          "[protocol][wol][codec]") {
    NumericReply reply;
    reply.server_name = "wol.ea.com";
    reply.code        = 376;
    reply.target      = "PlayerOne";
    reply.text        = "End of /MOTD command.";

    ServerMessage msg = reply;
    const auto wire = encode_server(msg);
    REQUIRE(wire == ":wol.ea.com 376 PlayerOne :End of /MOTD command.\r\n");
}

TEST_CASE("wol codec: ServerMessage variant encode — RawLine",
          "[protocol][wol][codec]") {
    RawLine raw;
    raw.line = "ERROR :Closing Link";

    ServerMessage msg = raw;
    const auto wire = encode_server(msg);
    REQUIRE(wire == "ERROR :Closing Link\r\n");
}

// ===========================================================================
// Round-trip: encode NumericReply then verify wire format
// ===========================================================================

TEST_CASE("wol codec: NumericReply 3-digit code formatting", "[protocol][wol][codec]") {
    // Verify zero-padding for codes < 100.
    NumericReply r1;
    r1.server_name = "s";
    r1.code        = 1;
    r1.target      = "t";
    r1.text        = "x";
    REQUIRE(encode_server(r1) == ":s 001 t :x\r\n");

    NumericReply r2;
    r2.server_name = "s";
    r2.code        = 42;
    r2.target      = "t";
    r2.text        = "x";
    REQUIRE(encode_server(r2) == ":s 042 t :x\r\n");

    NumericReply r3;
    r3.server_name = "s";
    r3.code        = 451;
    r3.target      = "t";
    r3.text        = "x";
    REQUIRE(encode_server(r3) == ":s 451 t :x\r\n");
}
