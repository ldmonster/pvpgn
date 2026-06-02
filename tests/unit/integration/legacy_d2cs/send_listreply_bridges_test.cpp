// SPDX-License-Identifier: GPL-2.0-or-later
// Wire tests for R75 list-reply bridges:
//   D2CS_CLIENT_GAMELISTREPLY (0x05) and D2CS_CLIENT_GAMEINFOREPLY (0x06).

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "app/d2cs/legacy_d2cs_bridges/send_listreply_bridges.hpp"
#include "app/d2cs/legacy_d2cs_bridges/send_packet_bridge.hpp"

namespace ild = pvpgn::integration::legacy_d2cs;

namespace {

struct FakeSink {
    static inline int call_count = 0;
    static inline std::vector<unsigned char> last_bytes{};
    static void reset() noexcept { call_count = 0; last_bytes.clear(); }
    static int handler(void*, void const* b, unsigned int n) noexcept {
        ++call_count;
        last_bytes.assign(
            static_cast<unsigned char const*>(b),
            static_cast<unsigned char const*>(b) + n);
        return 1;
    }
};

struct ScopedSink {
    ild::SendPacketHandler prev = ild::get_send_packet_handler();
    ScopedSink() noexcept { FakeSink::reset(); ild::set_send_packet_handler(&FakeSink::handler); }
    ~ScopedSink() noexcept { ild::set_send_packet_handler(prev); }
};

inline void check_hdr(std::uint16_t exp_size, std::uint8_t exp_type) {
    REQUIRE(FakeSink::last_bytes.size() >= 3u);
    REQUIRE(FakeSink::last_bytes[0] == (exp_size & 0xff));
    REQUIRE(FakeSink::last_bytes[1] == ((exp_size >> 8) & 0xff));
    REQUIRE(FakeSink::last_bytes[2] == exp_type);
}

}  // namespace

TEST_CASE("d2cs GAMELISTREPLY entry encodes hdr + seqno + token + currchar + gameflag + 2 cstrings",
          "[integration][legacy_d2cs][send_listreply_bridges]") {
    ScopedSink scope;
    int m = 0;
    // game_name="g", game_desc="d" => hdr(3)+seqno(2)+token(4)+1+4 + 2 + 2 = 18
    REQUIRE(::pvpgn_v3_d2cs_send_gamelistreply(&m, 0x4321u, 0xdeadbeefu,
                3u, 0x04u, "g", "d", /*terminator=*/0) == 1);
    REQUIRE(FakeSink::last_bytes.size() == 18u);
    check_hdr(18, 0x05);
    REQUIRE(FakeSink::last_bytes[3] == 0x21);
    REQUIRE(FakeSink::last_bytes[4] == 0x43);
    REQUIRE(FakeSink::last_bytes[5] == 0xef);
    REQUIRE(FakeSink::last_bytes[6] == 0xbe);
    REQUIRE(FakeSink::last_bytes[7] == 0xad);
    REQUIRE(FakeSink::last_bytes[8] == 0xde);
    REQUIRE(FakeSink::last_bytes[9] == 0x03);  // currchar
    REQUIRE(FakeSink::last_bytes[10] == 0x04); // gameflag low
    REQUIRE(FakeSink::last_bytes[14] == 'g');
    REQUIRE(FakeSink::last_bytes[15] == 0x00);
    REQUIRE(FakeSink::last_bytes[16] == 'd');
    REQUIRE(FakeSink::last_bytes[17] == 0x00);
}

TEST_CASE("d2cs GAMELISTREPLY terminator emits 3 empty cstrings",
          "[integration][legacy_d2cs][send_listreply_bridges]") {
    ScopedSink scope;
    int m = 0;
    // hdr(3)+seqno(2)+token(4)+1+4 + 1 + 1 + 1 = 17
    REQUIRE(::pvpgn_v3_d2cs_send_gamelistreply(&m, 0u, 0u, 0u, 0u, "", "",
                /*terminator=*/1) == 1);
    REQUIRE(FakeSink::last_bytes.size() == 17u);
    check_hdr(17, 0x05);
    REQUIRE(FakeSink::last_bytes[14] == 0x00);
    REQUIRE(FakeSink::last_bytes[15] == 0x00);
    REQUIRE(FakeSink::last_bytes[16] == 0x00);
}

TEST_CASE("d2cs GAMEINFOREPLY encodes hdr + scalars + 32 byte tables + desc + names",
          "[integration][legacy_d2cs][send_listreply_bridges]") {
    ScopedSink scope;
    int m = 0;
    unsigned char chclass[16] = {0};
    unsigned char level[16]   = {0};
    chclass[0] = 0x05; level[0] = 0x42;
    char const* names[2] = {"a", "bb"};
    // 49 + desc("x"\0=2) + "a"\0(2) + "bb"\0(3) = 56
    REQUIRE(::pvpgn_v3_d2cs_send_gameinforeply(&m, 0x0102u,
                0x00000004u, 0x12345678u,
                10u, 2u, 4u, 2u,
                chclass, level, "x", names) == 1);
    REQUIRE(FakeSink::last_bytes.size() == 56u);
    check_hdr(56, 0x06);
    REQUIRE(FakeSink::last_bytes[3] == 0x02);  // seqno LE
    REQUIRE(FakeSink::last_bytes[4] == 0x01);
    REQUIRE(FakeSink::last_bytes[5] == 0x04);  // gameflag
    REQUIRE(FakeSink::last_bytes[9] == 0x78);  // etime LE byte0
    REQUIRE(FakeSink::last_bytes[12] == 0x12);
    REQUIRE(FakeSink::last_bytes[13] == 10);   // charlevel
    REQUIRE(FakeSink::last_bytes[14] == 2);    // leveldiff
    REQUIRE(FakeSink::last_bytes[15] == 4);    // maxchar
    REQUIRE(FakeSink::last_bytes[16] == 2);    // currchar
    REQUIRE(FakeSink::last_bytes[17] == 0x05); // chclass[0]
    REQUIRE(FakeSink::last_bytes[33] == 0x42); // level[0]
    REQUIRE(FakeSink::last_bytes[49] == 'x');
    REQUIRE(FakeSink::last_bytes[50] == 0x00);
    REQUIRE(FakeSink::last_bytes[51] == 'a');
    REQUIRE(FakeSink::last_bytes[52] == 0x00);
    REQUIRE(FakeSink::last_bytes[53] == 'b');
    REQUIRE(FakeSink::last_bytes[54] == 'b');
    REQUIRE(FakeSink::last_bytes[55] == 0x00);
}

TEST_CASE("d2cs listreply bridges reject null",
          "[integration][legacy_d2cs][send_listreply_bridges]") {
    ScopedSink scope;
    int m = 0;
    unsigned char buf[16] = {0};
    char const* names[1] = {nullptr};
    REQUIRE(::pvpgn_v3_d2cs_send_gamelistreply(nullptr, 0,0,0,0,"a","b",0) == 0);
    REQUIRE(::pvpgn_v3_d2cs_send_gameinforeply(nullptr, 0,0,0,0,0,0,0,buf,buf,"d",nullptr) == 0);
    REQUIRE(::pvpgn_v3_d2cs_send_gameinforeply(&m, 0,0,0,0,0,0,0,nullptr,buf,"d",nullptr) == 0);
    REQUIRE(::pvpgn_v3_d2cs_send_gameinforeply(&m, 0,0,0,0,0,0,1,buf,buf,"d",nullptr) == 0);
    REQUIRE(::pvpgn_v3_d2cs_send_gameinforeply(&m, 0,0,0,0,0,0,1,buf,buf,"d",names) == 0);
    REQUIRE(FakeSink::call_count == 0);
}
