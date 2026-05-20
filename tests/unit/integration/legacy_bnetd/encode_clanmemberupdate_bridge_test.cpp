// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_encode_clanmemberupdate`.
//
// Verifies the wire bytes produced for SERVER_CLANMEMBERUPDATE
// (SID 0x7F) match the layout expected by clan.cpp's legacy broadcast.

#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "integration/legacy_bnetd/encode_clanmemberupdate_bridge.hpp"
#include "integration/legacy_bnetd/send_packet_bridge.hpp"

namespace ila = pvpgn::integration::legacy_bnetd;

TEST_CASE("encode_clanmemberupdate: rejects null name",
          "[integration][legacy_bnetd][encode_clanmemberupdate_bridge]") {
    unsigned char buf[64]{};
    unsigned int  out = 0;
    REQUIRE(::pvpgn_v3_encode_clanmemberupdate(
                nullptr, 1, 1, "", buf, sizeof(buf), &out) == 0);
}

TEST_CASE("encode_clanmemberupdate: rejects empty name",
          "[integration][legacy_bnetd][encode_clanmemberupdate_bridge]") {
    unsigned char buf[64]{};
    unsigned int  out = 0;
    REQUIRE(::pvpgn_v3_encode_clanmemberupdate(
                "", 1, 1, "", buf, sizeof(buf), &out) == 0);
}

TEST_CASE("encode_clanmemberupdate: rejects null out_buf",
          "[integration][legacy_bnetd][encode_clanmemberupdate_bridge]") {
    unsigned int out = 0;
    REQUIRE(::pvpgn_v3_encode_clanmemberupdate(
                "alice", 1, 1, "", nullptr, 64, &out) == 0);
}

TEST_CASE("encode_clanmemberupdate: rejects null out_size",
          "[integration][legacy_bnetd][encode_clanmemberupdate_bridge]") {
    unsigned char buf[64]{};
    REQUIRE(::pvpgn_v3_encode_clanmemberupdate(
                "alice", 1, 1, "", buf, sizeof(buf), nullptr) == 0);
}

TEST_CASE("encode_clanmemberupdate: rejects undersized buffer",
          "[integration][legacy_bnetd][encode_clanmemberupdate_bridge]") {
    unsigned char buf[4]{};
    unsigned int  out = 0;
    // Need at least 4 (header) + 6 (name "alice\0") + 1 (status) + 1
    // (online) + 1 (empty cstring) = 13 bytes; 4 is far too small.
    REQUIRE(::pvpgn_v3_encode_clanmemberupdate(
                "alice", 1, 1, "", buf, sizeof(buf), &out) == 0);
}

TEST_CASE("encode_clanmemberupdate: byte parity for online member with clienttag",
          "[integration][legacy_bnetd][encode_clanmemberupdate_bridge]") {
    unsigned char buf[64]{};
    unsigned int  out = 0;

    // name="bob", status=0x02, online_flag=0x01, online_status="3RAW"
    // Wire bytes (BNet header + body):
    //   FF 7F LL LL   -- header (LL = total size LE)
    //   'b' 'o' 'b' 00 -- name cstring (4)
    //   02            -- status (1)
    //   01            -- online flag (1)
    //   '3' 'R' 'A' 'W' 00 -- online_status cstring (5)
    // Total = 4 + 4 + 1 + 1 + 5 = 15 bytes.
    REQUIRE(::pvpgn_v3_encode_clanmemberupdate(
                "bob", 0x02, 0x01, "3RAW", buf, sizeof(buf), &out) == 1);
    REQUIRE(out == 15u);

    std::vector<unsigned char> expected{
        0xFF, 0x7F, 0x0F, 0x00,
        'b', 'o', 'b', 0x00,
        0x02,
        0x01,
        '3', 'R', 'A', 'W', 0x00};
    std::vector<unsigned char> actual(buf, buf + out);
    REQUIRE(actual == expected);
}

TEST_CASE("encode_clanmemberupdate: byte parity for offline member (null clienttag -> empty)",
          "[integration][legacy_bnetd][encode_clanmemberupdate_bridge]") {
    unsigned char buf[64]{};
    unsigned int  out = 0;

    // name="z", status=0x04, online_flag=0x00, online_status=nullptr
    // Wire: FF 7F LL LL | 'z' 00 | 04 | 00 | 00
    // Total = 4 + 2 + 1 + 1 + 1 = 9 bytes.
    REQUIRE(::pvpgn_v3_encode_clanmemberupdate(
                "z", 0x04, 0x00, nullptr, buf, sizeof(buf), &out) == 1);
    REQUIRE(out == 9u);

    std::vector<unsigned char> expected{
        0xFF, 0x7F, 0x09, 0x00,
        'z', 0x00,
        0x04,
        0x00,
        0x00};
    std::vector<unsigned char> actual(buf, buf + out);
    REQUIRE(actual == expected);
}
