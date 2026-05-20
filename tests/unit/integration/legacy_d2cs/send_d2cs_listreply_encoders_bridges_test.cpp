// SPDX-License-Identifier: GPL-2.0-or-later
// R91: bridge tests for v3 LADDERREPLY (0x11) and CHARLISTREPLY (0x17)
// emission via `pvpgn_v3_d2cs_send_packet_try`.

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_d2cs/send_charlistreply_bridge.hpp"
#include "integration/legacy_d2cs/send_ladderreply_bridge.hpp"
#include "integration/legacy_d2cs/send_packet_bridge.hpp"

namespace ild = pvpgn::integration::legacy_d2cs;

namespace {

struct CapturingSink {
    static inline std::vector<std::vector<unsigned char>> packets{};
    static void reset() noexcept { packets.clear(); }
    static int handler(void*, void const* b, unsigned int n) noexcept {
        packets.emplace_back(
            static_cast<unsigned char const*>(b),
            static_cast<unsigned char const*>(b) + n);
        return 1;
    }
};

struct ScopedSink {
    ild::SendPacketHandler prev = ild::get_send_packet_handler();
    ScopedSink() noexcept {
        CapturingSink::reset();
        ild::set_send_packet_handler(&CapturingSink::handler);
    }
    ~ScopedSink() noexcept { ild::set_send_packet_handler(prev); }
};

inline std::uint16_t u16(std::vector<unsigned char> const& b, std::size_t i) {
    return static_cast<std::uint16_t>(b[i] | (b[i + 1] << 8));
}
inline std::uint32_t u32(std::vector<unsigned char> const& b, std::size_t i) {
    return static_cast<std::uint32_t>(b[i]) |
           (static_cast<std::uint32_t>(b[i + 1]) << 8) |
           (static_cast<std::uint32_t>(b[i + 2]) << 16) |
           (static_cast<std::uint32_t>(b[i + 3]) << 24);
}

pvpgn_v3_d2cs_ladder_entry make_le(std::uint32_t idx) {
    pvpgn_v3_d2cs_ladder_entry e{};
    e.exp_low  = 1000u + idx;
    e.exp_high = 0u;
    e.status   = static_cast<std::uint16_t>(0x0100u | idx);
    e.level    = static_cast<std::uint8_t>(40u + idx);
    e.u1       = 0u;
    std::memset(e.charname, 0, sizeof(e.charname));
    const std::string name = "ch" + std::to_string(idx);
    std::memcpy(e.charname, name.data(),
                std::min(name.size(), sizeof(e.charname)));
    return e;
}

}  // namespace

TEST_CASE("d2cs LADDERREPLY bridge: 14 entries -> single 410-byte packet",
          "[integration][legacy_d2cs][send_ladderreply_bridge]") {
    ScopedSink scope;
    int conn = 0;
    std::vector<pvpgn_v3_d2cs_ladder_entry> entries;
    for (std::uint32_t i = 0; i < 14; ++i) entries.push_back(make_le(i));

    REQUIRE(::pvpgn_v3_d2cs_send_ladderreply(
                &conn, /*type=*/0x01, /*start_pos=*/0,
                entries.data(), static_cast<unsigned int>(entries.size())) == 1);

    REQUIRE(CapturingSink::packets.size() == 1u);
    const auto& p = CapturingSink::packets[0];
    REQUIRE(p.size() == 410u);
    CHECK(u16(p, 0) == 410);
    CHECK(p[2] == 0x11);          // packet type
    CHECK(p[3] == 0x01);          // ladder type
    CHECK(u16(p, 4) == 400);      // total_len
    CHECK(u16(p, 6) == 400);      // curr_len
    CHECK(u32(p, 14) == 14u);     // count1
    CHECK(u32(p, 18) == 14u);     // count2
}

TEST_CASE("d2cs LADDERREPLY bridge: 15 entries -> 2 packets",
          "[integration][legacy_d2cs][send_ladderreply_bridge]") {
    ScopedSink scope;
    int conn = 0;
    std::vector<pvpgn_v3_d2cs_ladder_entry> entries;
    for (std::uint32_t i = 0; i < 15; ++i) entries.push_back(make_le(i));

    REQUIRE(::pvpgn_v3_d2cs_send_ladderreply(
                &conn, 0x02, 7, entries.data(),
                static_cast<unsigned int>(entries.size())) == 1);

    REQUIRE(CapturingSink::packets.size() == 2u);
    CHECK(CapturingSink::packets[0].size() == 410u);
    CHECK(CapturingSink::packets[1].size() == 42u);
    CHECK(u16(CapturingSink::packets[0], 10) == 7);  // start_pos
    CHECK(u16(CapturingSink::packets[1], 8) == 400); // cont_len
}

TEST_CASE("d2cs LADDERREPLY bridge: null conn returns 0",
          "[integration][legacy_d2cs][send_ladderreply_bridge]") {
    ScopedSink scope;
    pvpgn_v3_d2cs_ladder_entry e = make_le(0);
    CHECK(::pvpgn_v3_d2cs_send_ladderreply(nullptr, 0, 0, &e, 1) == 0);
    CHECK(CapturingSink::packets.empty());
}

TEST_CASE("d2cs LADDERREPLY bridge: zero entries still emits a packet",
          "[integration][legacy_d2cs][send_ladderreply_bridge]") {
    ScopedSink scope;
    int conn = 0;
    REQUIRE(::pvpgn_v3_d2cs_send_ladderreply(
                &conn, 0x05, 0, nullptr, 0) == 1);
    REQUIRE(CapturingSink::packets.size() == 1u);
    CHECK(CapturingSink::packets[0].size() == 10u);
    CHECK(CapturingSink::packets[0][2] == 0x11);
    CHECK(CapturingSink::packets[0][3] == 0x05);
}

TEST_CASE("d2cs CHARLISTREPLY bridge: empty entry list",
          "[integration][legacy_d2cs][send_charlistreply_bridge]") {
    ScopedSink scope;
    int conn = 0;
    REQUIRE(::pvpgn_v3_d2cs_send_charlistreply(
                &conn, /*maxchar_field=*/18, nullptr, 0) == 1);
    REQUIRE(CapturingSink::packets.size() == 1u);
    const auto& p = CapturingSink::packets[0];
    REQUIRE(p.size() == 11u);
    CHECK(p[2] == 0x17);
    CHECK(u16(p, 3) == 18);
    CHECK(u16(p, 5) == 0);
    CHECK(u16(p, 9) == 0);
}

TEST_CASE("d2cs CHARLISTREPLY bridge: two entries with portraits",
          "[integration][legacy_d2cs][send_charlistreply_bridge]") {
    ScopedSink scope;
    int conn = 0;
    unsigned char port1[] = {0x10, 0x20};
    unsigned char port2[] = {0x33};
    pvpgn_v3_d2cs_charlist_entry es[2]{};
    es[0].charname = "Alice";
    es[0].portrait = port1;
    es[0].portrait_len = sizeof(port1);
    es[1].charname = "Bob";
    es[1].portrait = port2;
    es[1].portrait_len = sizeof(port2);

    REQUIRE(::pvpgn_v3_d2cs_send_charlistreply(&conn, 8, es, 2) == 1);
    REQUIRE(CapturingSink::packets.size() == 1u);
    const auto& p = CapturingSink::packets[0];

    // 11 base + ("Alice\0"=6 + 2 portrait + NUL=3 -> 9) + ("Bob\0"=4 + 1 + NUL=2 -> 6) = 26
    REQUIRE(p.size() == 26u);
    CHECK(u16(p, 3) == 8);   // maxchar
    CHECK(u16(p, 5) == 2);   // currchar
    CHECK(u16(p, 9) == 2);   // currchar2

    // Alice block at offset 11..19
    const char* alice = "Alice";
    for (std::size_t i = 0; i < 5; ++i) CHECK(p[11 + i] == static_cast<unsigned char>(alice[i]));
    CHECK(p[16] == 0x00);
    CHECK(p[17] == 0x10);
    CHECK(p[18] == 0x20);
    CHECK(p[19] == 0x00);  // portrait NUL terminator
    // Bob block at offset 20..25
    CHECK(p[20] == 'B');
    CHECK(p[21] == 'o');
    CHECK(p[22] == 'b');
    CHECK(p[23] == 0x00);
    CHECK(p[24] == 0x33);
    CHECK(p[25] == 0x00);
}

TEST_CASE("d2cs CHARLISTREPLY bridge: null conn returns 0",
          "[integration][legacy_d2cs][send_charlistreply_bridge]") {
    ScopedSink scope;
    CHECK(::pvpgn_v3_d2cs_send_charlistreply(nullptr, 0, nullptr, 0) == 0);
    CHECK(CapturingSink::packets.empty());
}
