// SPDX-License-Identifier: GPL-2.0-or-later
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "protocol/telnet/codec.hpp"

using namespace pvpgn;
using namespace pvpgn::protocol::telnet;

TEST_CASE("telnet: try_parse_line splits CRLF", "[protocol][telnet]") {
    auto f = try_parse_line("help\r\nquit\r\n");
    REQUIRE(f.has_value());
    REQUIRE(std::string{f.value().line} == "help");
    REQUIRE(f.value().consumed == 6);
}

TEST_CASE("telnet: try_parse_line tolerates bare LF",
          "[protocol][telnet]") {
    auto f = try_parse_line("help\n");
    REQUIRE(f.has_value());
    REQUIRE(std::string{f.value().line} == "help");
}

TEST_CASE("telnet: tokenise splits on whitespace",
          "[protocol][telnet]") {
    auto c = tokenise("  kick   alice   spamming  ");
    REQUIRE(c.verb == "kick");
    REQUIRE(c.args.size() == 2);
    REQUIRE(c.args[0] == "alice");
    REQUIRE(c.args[1] == "spamming");
}

TEST_CASE("telnet: tokenise empty line yields empty verb",
          "[protocol][telnet]") {
    auto c = tokenise("");
    REQUIRE(c.verb.empty());
    REQUIRE(c.args.empty());
}

TEST_CASE("telnet: write_line appends CRLF", "[protocol][telnet]") {
    protocol::Writer w;
    write_line(w, "OK");
    auto v = w.view();
    REQUIRE(v.size() == 4);
    REQUIRE(static_cast<char>(v[2]) == '\r');
    REQUIRE(static_cast<char>(v[3]) == '\n');
}
