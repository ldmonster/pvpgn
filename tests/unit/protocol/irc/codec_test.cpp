// SPDX-License-Identifier: GPL-2.0-or-later
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "protocol/irc/codec.hpp"

using namespace pvpgn;
using protocol::irc::Message;

TEST_CASE("irc: try_parse_line splits at CRLF and reports consumed",
          "[protocol][irc]") {
    std::string buf = "PING :foo\r\nNICK bob\r\n";
    auto f = protocol::irc::try_parse_line(buf);
    REQUIRE(f.has_value());
    REQUIRE(std::string{f.value().line} == "PING :foo");
    REQUIRE(f.value().consumed == 11);

    auto rest = std::string_view{buf}.substr(f.value().consumed);
    auto g = protocol::irc::try_parse_line(rest);
    REQUIRE(g.has_value());
    REQUIRE(std::string{g.value().line} == "NICK bob");
}

TEST_CASE("irc: try_parse_line tolerates bare LF", "[protocol][irc]") {
    auto f = protocol::irc::try_parse_line("PING :legacy\n");
    REQUIRE(f.has_value());
    REQUIRE(std::string{f.value().line} == "PING :legacy");
    REQUIRE(f.value().consumed == 13);
}

TEST_CASE("irc: try_parse_line returns OutOfRange when no terminator yet",
          "[protocol][irc]") {
    auto f = protocol::irc::try_parse_line("NICK part");
    REQUIRE_FALSE(f.has_value());
    REQUIRE(f.error().code() == core::StatusCode::OutOfRange);
}

TEST_CASE("irc: decode PRIVMSG with trailing param", "[protocol][irc]") {
    auto m = protocol::irc::decode(":alice!u@h PRIVMSG #pvpgn :hello world");
    REQUIRE(m.has_value());
    REQUIRE(m.value().prefix == "alice!u@h");
    REQUIRE(m.value().command == "PRIVMSG");
    REQUIRE(m.value().params.size() == 2);
    REQUIRE(m.value().params[0] == "#pvpgn");
    REQUIRE(m.value().params[1] == "hello world");
}

TEST_CASE("irc: decode upper-cases lowercase commands",
          "[protocol][irc]") {
    auto m = protocol::irc::decode("nick bob");
    REQUIRE(m.has_value());
    REQUIRE(m.value().command == "NICK");
    REQUIRE(m.value().params.size() == 1);
    REQUIRE(m.value().params[0] == "bob");
}

TEST_CASE("irc: decode rejects empty line and prefix-only",
          "[protocol][irc]") {
    REQUIRE_FALSE(protocol::irc::decode("").has_value());
    REQUIRE_FALSE(protocol::irc::decode(":alice").has_value());
}

TEST_CASE("irc: encode round-trip with prefix + trailing",
          "[protocol][irc]") {
    Message in{"alice!u@h", "PRIVMSG", {"#pvpgn", "hello world"}};
    const auto wire = protocol::irc::encode_to_string(in);
    REQUIRE(wire == ":alice!u@h PRIVMSG #pvpgn :hello world\r\n");

    auto f = protocol::irc::try_parse_line(wire);
    REQUIRE(f.has_value());
    auto m = protocol::irc::decode(f.value().line);
    REQUIRE(m.has_value());
    REQUIRE(m.value() == in);
}

TEST_CASE("irc: encode marks a last param starting with ':' as trailing",
          "[protocol][irc]") {
    Message in{"", "TOPIC", {"#pvpgn", ":announce"}};
    const auto wire = protocol::irc::encode_to_string(in);
    REQUIRE(wire == "TOPIC #pvpgn ::announce\r\n");
}

TEST_CASE("irc: encode marks empty last param as trailing",
          "[protocol][irc]") {
    Message in{"", "TOPIC", {"#pvpgn", ""}};
    const auto wire = protocol::irc::encode_to_string(in);
    REQUIRE(wire == "TOPIC #pvpgn :\r\n");
}

TEST_CASE("irc: encode rejects empty command", "[protocol][irc]") {
    Message bad_msg{"", "", {"foo"}};
    protocol::Writer w;
    auto s = protocol::irc::encode(w, bad_msg);
    REQUIRE_FALSE(s.has_value());
}
