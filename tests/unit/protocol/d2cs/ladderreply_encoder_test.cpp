// SPDX-License-Identifier: GPL-2.0-or-later
// Parity test for the v3 D2CS_CLIENT_LADDERREPLY encoder.
//
// Drives the byte layout against the legacy `d2cs_send_client_ladder`
// emitter in `src/d2cs/handle_d2cs.cpp`, including the well-known
// `packet_set_size(rpacket, packet_get_size(rpacket) - 4)` quirk on
// the first packet.

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#include "protocol/d2cs/ladderreply_encoder.hpp"

namespace lr = pvpgn::protocol::d2cs::ladderreply;

namespace {

lr::LadderInfo make_entry(std::uint32_t idx) {
    lr::LadderInfo li{};
    li.exp_low  = 1000u + idx;
    li.exp_high = 0u;            // legacy: always zero
    li.status   = static_cast<std::uint16_t>(0x0100u | idx);
    li.level    = static_cast<std::uint8_t>(40u + idx);
    li.u1       = 0u;            // legacy: always zero
    li.charname.fill('\0');
    const std::string name = "char" + std::to_string(idx);
    std::memcpy(li.charname.data(), name.data(),
                std::min(name.size(), li.charname.size()));
    return li;
}

std::uint16_t read_u16le(const std::vector<std::byte>& b, std::size_t off) {
    return static_cast<std::uint16_t>(
        static_cast<std::uint8_t>(b[off]) |
        (static_cast<std::uint8_t>(b[off + 1]) << 8));
}
std::uint32_t read_u32le(const std::vector<std::byte>& b, std::size_t off) {
    return static_cast<std::uint32_t>(
               static_cast<std::uint8_t>(b[off])) |
           (static_cast<std::uint32_t>(
                static_cast<std::uint8_t>(b[off + 1])) << 8) |
           (static_cast<std::uint32_t>(
                static_cast<std::uint8_t>(b[off + 2])) << 16) |
           (static_cast<std::uint32_t>(
                static_cast<std::uint8_t>(b[off + 3])) << 24);
}

}  // namespace

TEST_CASE("ladderreply: empty -> single 10-byte packet with all zero lengths",
          "[protocol][d2cs][ladderreply]") {
    auto pkts = lr::encode(/*type=*/0x05, /*start_pos=*/0, /*entries=*/{});
    REQUIRE(pkts.size() == 1);
    const auto& b = pkts[0].bytes;
    REQUIRE(b.size() == lr::kReplyBaseSize);
    CHECK(read_u16le(b, 0) == 10);                          // size
    CHECK(static_cast<std::uint8_t>(b[2]) == lr::kPacketType);
    CHECK(static_cast<std::uint8_t>(b[3]) == 0x05);         // ladder type
    CHECK(read_u16le(b, 4) == 0);                           // total_len
    CHECK(read_u16le(b, 6) == 0);                           // curr_len
    CHECK(read_u16le(b, 8) == 0);                           // cont_len
}

TEST_CASE("ladderreply: 1 entry -> single packet, -4 truncation cuts into entry",
          "[protocol][d2cs][ladderreply]") {
    std::vector<lr::LadderInfo> entries{make_entry(0)};
    auto pkts = lr::encode(0x01, /*start_pos=*/7, entries);
    REQUIRE(pkts.size() == 1);
    const auto& b = pkts[0].bytes;

    // total_len_full = 1*28 + 8 + 4*1 = 40; total_len = 36.
    // First-packet body before truncation = 40; after = 36.
    // wire size = 10 + 36 = 46.
    CHECK(b.size() == 46u);
    CHECK(read_u16le(b, 0) == 46);
    CHECK(static_cast<std::uint8_t>(b[2]) == lr::kPacketType);
    CHECK(static_cast<std::uint8_t>(b[3]) == 0x01);
    CHECK(read_u16le(b, 4) == 36);  // total_len
    CHECK(read_u16le(b, 6) == 36);  // curr_len
    CHECK(read_u16le(b, 8) == 0);   // cont_len

    // ladderheader at offset 10..17
    CHECK(read_u16le(b, 10) == 7);   // start_pos
    CHECK(read_u16le(b, 12) == 0);   // u1
    CHECK(read_u32le(b, 14) == 1);   // count1

    // infoheader at offset 18..21
    CHECK(read_u32le(b, 18) == 1);   // count2 = count on first packet

    // entry begins at offset 22; legacy truncation strips the LAST 4
    // bytes of the body, so only 24 of the entry's 28 bytes survive.
    CHECK(read_u32le(b, 22) == 1000u);              // exp_low
    CHECK(read_u32le(b, 26) == 0u);                 // exp_high
    CHECK(read_u16le(b, 30) == 0x0100u);            // status
    CHECK(static_cast<std::uint8_t>(b[32]) == 40u); // level
    CHECK(static_cast<std::uint8_t>(b[33]) == 0u);  // u1
    // charname: "char0" + 11 zero bytes, but only 12 of 16 bytes survive
    CHECK(static_cast<char>(b[34]) == 'c');
    CHECK(static_cast<char>(b[35]) == 'h');
    CHECK(static_cast<char>(b[36]) == 'a');
    CHECK(static_cast<char>(b[37]) == 'r');
    CHECK(static_cast<char>(b[38]) == '0');
    // bytes 39..45 are the surviving charname tail (NUL padding).
    for (std::size_t i = 39; i < 46; ++i) {
        CHECK(static_cast<std::uint8_t>(b[i]) == 0u);
    }
}

TEST_CASE("ladderreply: 14 entries fits one packet, total_len = 400",
          "[protocol][d2cs][ladderreply]") {
    std::vector<lr::LadderInfo> entries;
    for (std::uint32_t i = 0; i < 14; ++i) entries.push_back(make_entry(i));
    auto pkts = lr::encode(0x02, /*start_pos=*/0, entries);
    REQUIRE(pkts.size() == 1);
    const auto& b = pkts[0].bytes;

    // total_len_full = 14*28 + 8 + 4 = 404; total_len = 400
    // wire size = 10 + 404 - 4 = 410
    CHECK(b.size() == 410u);
    CHECK(read_u16le(b, 0) == 410);
    CHECK(read_u16le(b, 4) == 400);  // total_len
    CHECK(read_u16le(b, 6) == 400);  // curr_len  (matches total on single-packet)
    CHECK(read_u16le(b, 8) == 0);    // cont_len
    CHECK(read_u32le(b, 14) == 14u); // count1
    CHECK(read_u32le(b, 18) == 14u); // count2 on first packet
}

TEST_CASE("ladderreply: 15 entries -> two packets with continuation accounting",
          "[protocol][d2cs][ladderreply]") {
    std::vector<lr::LadderInfo> entries;
    for (std::uint32_t i = 0; i < 15; ++i) entries.push_back(make_entry(i));
    auto pkts = lr::encode(0x03, /*start_pos=*/0, entries);
    REQUIRE(pkts.size() == 2);

    // total_len_full = 15*28 + 8 + 4*2 = 436; total_len = 432
    const auto& p0 = pkts[0].bytes;
    const auto& p1 = pkts[1].bytes;

    // Packet 0: 14 entries, first-packet quirks apply.
    // body before trunc = 8 + 4 + 14*28 = 404; after = 400.
    // wire size = 10 + 400 = 410.  curr_len = 400.
    CHECK(p0.size() == 410u);
    CHECK(read_u16le(p0, 4) == 432);  // total_len
    CHECK(read_u16le(p0, 6) == 400);  // curr_len
    CHECK(read_u16le(p0, 8) == 0);    // cont_len
    CHECK(read_u32le(p0, 14) == 15u); // count1 = total
    CHECK(read_u32le(p0, 18) == 15u); // count2 on first packet

    // Packet 1: 1 entry, no ladderheader, count2 = 0, no truncation.
    // body = 4 (infoheader) + 28 = 32; wire size = 10 + 32 = 42.
    CHECK(p1.size() == 42u);
    CHECK(read_u16le(p1, 4) == 432);  // total_len repeated
    CHECK(read_u16le(p1, 6) == 32);   // curr_len for this packet
    CHECK(read_u16le(p1, 8) == 400);  // cont_len = sum of prior curr_lens
    CHECK(read_u32le(p1, 10) == 0u);  // count2 = 0 on continuation
    CHECK(read_u32le(p1, 14) == 1014u); // entry 14: exp_low = 1000+14
}

TEST_CASE("ladderreply: 28 entries -> exactly two full packets",
          "[protocol][d2cs][ladderreply]") {
    std::vector<lr::LadderInfo> entries;
    for (std::uint32_t i = 0; i < 28; ++i) entries.push_back(make_entry(i));
    auto pkts = lr::encode(0x04, 0, entries);
    REQUIRE(pkts.size() == 2);

    // total_len_full = 28*28 + 8 + 4*2 = 800; total_len = 796
    // Packet 0: body_before = 8+4+14*28 = 404; trunc -> 400; wire = 410.
    // Packet 1: body = 4 + 14*28 = 396; wire = 406.
    CHECK(pkts[0].bytes.size() == 410u);
    CHECK(pkts[1].bytes.size() == 406u);
    CHECK(read_u16le(pkts[0].bytes, 4) == 796);
    CHECK(read_u16le(pkts[0].bytes, 6) == 400);
    CHECK(read_u16le(pkts[1].bytes, 6) == 396);
    CHECK(read_u16le(pkts[1].bytes, 8) == 400);  // cont_len
}

TEST_CASE("ladderreply: start_pos preserved in ladderheader",
          "[protocol][d2cs][ladderreply]") {
    std::vector<lr::LadderInfo> entries{make_entry(0), make_entry(1)};
    auto pkts = lr::encode(0x06, /*start_pos=*/42, entries);
    REQUIRE(pkts.size() == 1);
    CHECK(read_u16le(pkts[0].bytes, 10) == 42u);
}
