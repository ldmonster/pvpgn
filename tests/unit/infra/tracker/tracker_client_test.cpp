// SPDX-License-Identifier: GPL-2.0-or-later

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <string>

#include "infra/tracker/tracker_client.hpp"

using namespace pvpgn::infra::tracker;

namespace {

std::uint16_t rd_be16(const std::byte* p) {
    return static_cast<std::uint16_t>(
        (static_cast<unsigned>(std::to_integer<unsigned char>(p[0])) << 8) |
         static_cast<unsigned>(std::to_integer<unsigned char>(p[1])));
}

std::uint32_t rd_be32(const std::byte* p) {
    return (static_cast<std::uint32_t>(std::to_integer<unsigned char>(p[0])) << 24) |
           (static_cast<std::uint32_t>(std::to_integer<unsigned char>(p[1])) << 16) |
           (static_cast<std::uint32_t>(std::to_integer<unsigned char>(p[2])) <<  8) |
            static_cast<std::uint32_t>(std::to_integer<unsigned char>(p[3]));
}

}  // namespace

TEST_CASE("trackpacket has fixed 464-byte size", "[infra][tracker]") {
    ReportStats s{};
    auto buf = encode_trackpacket(6112, s);
    REQUIRE(buf.size() == kTrackPacketSize);
}

TEST_CASE("trackpacket header fields are big-endian", "[infra][tracker]") {
    ReportStats s{};
    s.users        = 0x11223344;
    s.channels     = 5;
    s.games        = 6;
    s.uptime       = 0x000000FF;
    s.total_games  = 0x12345678;
    s.total_logins = 9;
    s.flags        = kTrackFlagPrivate;

    auto buf = encode_trackpacket(0xBEEF, s);
    const std::byte* p = buf.data();

    CHECK(rd_be16(p +   0) == kTrackVersion);
    CHECK(rd_be16(p +   2) == 0xBEEF);
    CHECK(rd_be32(p +   4) == kTrackFlagPrivate);
    CHECK(rd_be32(p + 440) == 0x11223344u);
    CHECK(rd_be32(p + 444) == 5u);
    CHECK(rd_be32(p + 448) == 6u);
    CHECK(rd_be32(p + 452) == 0xFFu);
    CHECK(rd_be32(p + 456) == 0x12345678u);
    CHECK(rd_be32(p + 460) == 9u);
}

TEST_CASE("trackpacket NUL-pads strings inside their slots", "[infra][tracker]") {
    ReportStats s{};
    s.software        = "pvpgn";
    s.version         = "3.0.0";
    s.platform        = "Linux";
    s.server_desc     = "test";
    s.server_location = "earth";
    s.server_url      = "http://example.com";
    s.contact_name    = "alice";
    s.contact_email   = "a@b";

    auto buf = encode_trackpacket(0, s);
    const std::byte* p = buf.data();

    auto slot = [&](std::size_t off, std::size_t cap) {
        std::string r;
        for (std::size_t i = 0; i < cap; ++i) {
            unsigned char c = std::to_integer<unsigned char>(p[off + i]);
            if (c == 0) break;
            r.push_back(static_cast<char>(c));
        }
        return r;
    };

    CHECK(slot(  8, 32) == "pvpgn");
    CHECK(slot( 40, 16) == "3.0.0");
    CHECK(slot( 56, 32) == "Linux");
    CHECK(slot( 88, 64) == "test");
    CHECK(slot(152, 64) == "earth");
    CHECK(slot(216, 96) == "http://example.com");
    CHECK(slot(312, 64) == "alice");
    CHECK(slot(376, 64) == "a@b");

    // First byte after each NUL-terminated slot's used range is zero.
    CHECK(std::to_integer<unsigned char>(p[8 + 5]) == 0);
}

TEST_CASE("trackpacket truncates oversize strings keeping NUL", "[infra][tracker]") {
    ReportStats s{};
    s.version = std::string(64, 'X');  // far longer than 16-byte slot
    auto buf = encode_trackpacket(0, s);
    const std::byte* p = buf.data();
    // Slot 40..55: 15 X's then NUL.
    for (std::size_t i = 0; i < 15; ++i) {
        CHECK(std::to_integer<unsigned char>(p[40 + i]) == 'X');
    }
    CHECK(std::to_integer<unsigned char>(p[40 + 15]) == 0);
}

TEST_CASE("parse_servers handles host, host:port, list", "[infra][tracker]") {
    auto v = parse_servers("a.example,b.example:1234, c:9999 ,bad:,,d");
    REQUIRE(v.size() == 4);
    CHECK(v[0].host == "a.example");
    CHECK(v[0].port == kTrackDefaultPort);
    CHECK(v[1].host == "b.example");
    CHECK(v[1].port == 1234);
    CHECK(v[2].host == "c");
    CHECK(v[2].port == 9999);
    CHECK(v[3].host == "d");
    CHECK(v[3].port == kTrackDefaultPort);
}

TEST_CASE("parse_servers honours explicit default_port", "[infra][tracker]") {
    auto v = parse_servers("only-host", 7777);
    REQUIRE(v.size() == 1);
    CHECK(v[0].host == "only-host");
    CHECK(v[0].port == 7777);
}

TEST_CASE("parse_servers returns empty for empty input", "[infra][tracker]") {
    CHECK(parse_servers("").empty());
    CHECK(parse_servers(", , ,").empty());
}
