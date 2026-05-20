// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for the consolidated clan encode/send bridges
// (`clan_bridges.{hpp,cpp}`): CLANMEMBERLIST_REPLY,
// CLAN_CREATEREPLY, CLAN_CLANACK (send + encode), CLANQUITNOTIFY.

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/clan_bridges.hpp"
#include "integration/legacy_bnetd/send_packet_bridge.hpp"

namespace ila = pvpgn::integration::legacy_bnetd;

namespace {

struct FakeSink {
    static inline int     return_value = 1;
    static inline int     call_count   = 0;
    static inline void*   last_conn    = nullptr;
    static inline std::vector<unsigned char> last_bytes{};

    static void reset() noexcept {
        return_value = 1;
        call_count   = 0;
        last_conn    = nullptr;
        last_bytes.clear();
    }

    static int handler(void* conn_ptr,
                       void const* bytes,
                       unsigned int size) noexcept {
        ++call_count;
        last_conn = conn_ptr;
        last_bytes.assign(
            static_cast<unsigned char const*>(bytes),
            static_cast<unsigned char const*>(bytes) + size);
        return return_value;
    }
};

struct ScopedSink {
    ila::SendPacketHandler prev = ila::get_send_packet_handler();
    ScopedSink() noexcept {
        FakeSink::reset();
        ila::set_send_packet_handler(&FakeSink::handler);
    }
    ~ScopedSink() noexcept { ila::set_send_packet_handler(prev); }
};

}  // namespace

// ---- CLANMEMBERLIST_REPLY ----------------------------------------------

TEST_CASE("encode_clanmemberlist_reply: empty list",
          "[integration][legacy_bnetd][clan_bridges]") {
    unsigned char buf[64]{};
    unsigned int  out = 0;
    REQUIRE(::pvpgn_v3_encode_clanmemberlist_reply(
                0x11223344u, 0u,
                nullptr, nullptr, nullptr, nullptr,
                buf, sizeof(buf), &out) == 1);
    // header(4) + cookie(4) + member_count(1) = 9
    REQUIRE(out == 9u);
    std::vector<unsigned char> expected{
        0xFF, 0x7D, 0x09, 0x00,
        0x44, 0x33, 0x22, 0x11,
        0x00};
    std::vector<unsigned char> actual(buf, buf + out);
    REQUIRE(actual == expected);
}

TEST_CASE("encode_clanmemberlist_reply: two members byte parity",
          "[integration][legacy_bnetd][clan_bridges]") {
    char const*    names[]    = {"alice", "bob"};
    unsigned char  statuses[] = {0x02, 0x04};
    unsigned char  online[]   = {0x01, 0x00};
    char const*    cts[]      = {"3RAW", nullptr};

    unsigned char buf[128]{};
    unsigned int  out = 0;
    REQUIRE(::pvpgn_v3_encode_clanmemberlist_reply(
                0xDEADBEEFu, 2u,
                names, statuses, online, cts,
                buf, sizeof(buf), &out) == 1);
    // header(4) + cookie(4) + member_count(1)
    //  + alice\0(6) + 02 + 01 + 3RAW\0(5) = 13
    //  + bob\0(4)   + 04 + 00 + \0(1)     = 7
    // total = 4 + 4 + 1 + 13 + 7 = 29
    REQUIRE(out == 29u);
    std::vector<unsigned char> expected{
        0xFF, 0x7D, 0x1D, 0x00,
        0xEF, 0xBE, 0xAD, 0xDE,
        0x02,
        'a','l','i','c','e', 0x00, 0x02, 0x01, '3','R','A','W', 0x00,
        'b','o','b', 0x00, 0x04, 0x00, 0x00};
    std::vector<unsigned char> actual(buf, buf + out);
    REQUIRE(actual == expected);
}

TEST_CASE("encode_clanmemberlist_reply: rejects null name in list",
          "[integration][legacy_bnetd][clan_bridges]") {
    char const*   names[]    = {nullptr};
    unsigned char statuses[] = {0x01};
    unsigned char online[]   = {0x00};
    unsigned char buf[64]{};
    unsigned int  out = 0;
    REQUIRE(::pvpgn_v3_encode_clanmemberlist_reply(
                0u, 1u, names, statuses, online, nullptr,
                buf, sizeof(buf), &out) == 0);
}

// ---- CLAN_CREATEREPLY --------------------------------------------------

TEST_CASE("encode_clan_createreply: zero friends byte parity",
          "[integration][legacy_bnetd][clan_bridges]") {
    unsigned char buf[32]{};
    unsigned int  out = 0;
    REQUIRE(::pvpgn_v3_encode_clan_createreply(
                0x01020304u, 0x01, 0u, nullptr,
                buf, sizeof(buf), &out) == 1);
    // header(4) + cookie(4) + check_result(1) + friend_count(1) = 10
    REQUIRE(out == 10u);
    std::vector<unsigned char> expected{
        0xFF, 0x70, 0x0A, 0x00,
        0x04, 0x03, 0x02, 0x01,
        0x01, 0x00};
    std::vector<unsigned char> actual(buf, buf + out);
    REQUIRE(actual == expected);
}

TEST_CASE("encode_clan_createreply: two friend names byte parity",
          "[integration][legacy_bnetd][clan_bridges]") {
    char const* friends[] = {"X", "Yo"};
    unsigned char buf[32]{};
    unsigned int  out = 0;
    REQUIRE(::pvpgn_v3_encode_clan_createreply(
                0u, 0x00, 2u, friends,
                buf, sizeof(buf), &out) == 1);
    // 4 + 4 + 1 + 1 + 2 + 3 = 15
    REQUIRE(out == 15u);
    std::vector<unsigned char> expected{
        0xFF, 0x70, 0x0F, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x02,
        'X', 0x00,
        'Y', 'o', 0x00};
    std::vector<unsigned char> actual(buf, buf + out);
    REQUIRE(actual == expected);
}

// ---- CLAN_CLANACK (send + encode) --------------------------------------

TEST_CASE("encode_clan_clanack: byte parity",
          "[integration][legacy_bnetd][clan_bridges]") {
    unsigned char buf[16]{};
    unsigned int  out = 0;
    REQUIRE(::pvpgn_v3_encode_clan_clanack(
                0x00, 0xAABBCCDDu, 0x03,
                buf, sizeof(buf), &out) == 1);
    // header(4) + unknown1(1) + clantag(4) + status(1) = 10
    REQUIRE(out == 10u);
    std::vector<unsigned char> expected{
        0xFF, 0x75, 0x0A, 0x00,
        0x00,
        0xDD, 0xCC, 0xBB, 0xAA,
        0x03};
    std::vector<unsigned char> actual(buf, buf + out);
    REQUIRE(actual == expected);
}

TEST_CASE("send_clan_clanack: returns 0 with no sink",
          "[integration][legacy_bnetd][clan_bridges]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_clan_clanack(&marker, 0, 0xAABBCCDD, 1) == 0);
    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_clan_clanack: rejects null conn",
          "[integration][legacy_bnetd][clan_bridges]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_clan_clanack(nullptr, 0, 0, 0) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_clan_clanack: dispatches encoded bytes",
          "[integration][legacy_bnetd][clan_bridges]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_clan_clanack(
                &marker, 0x00, 0xAABBCCDDu, 0x02) == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);
    REQUIRE(FakeSink::last_bytes.size() == 10u);
    REQUIRE(FakeSink::last_bytes[1] == 0x75);  // SID byte
}

// ---- CLANQUITNOTIFY ----------------------------------------------------

TEST_CASE("encode_clan_quitnotify: byte parity",
          "[integration][legacy_bnetd][clan_bridges]") {
    unsigned char buf[16]{};
    unsigned int  out = 0;
    REQUIRE(::pvpgn_v3_encode_clan_quitnotify(
                0x01, buf, sizeof(buf), &out) == 1);
    // header(4) + status(1) = 5
    REQUIRE(out == 5u);
    std::vector<unsigned char> expected{
        0xFF, 0x76, 0x05, 0x00,
        0x01};
    std::vector<unsigned char> actual(buf, buf + out);
    REQUIRE(actual == expected);
}
