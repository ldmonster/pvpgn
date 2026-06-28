// SPDX-License-Identifier: GPL-2.0-or-later
// Wire tests for d2cs<->bnetd internal bridges:
//   pvpgn_v3_d2cs_send_init_bnetd            (1-byte init class 0x65)
//   pvpgn_v3_d2cs_send_authreply_bnetd       (8B hdr + version + realm\0)
//   pvpgn_v3_d2cs_send_gameinforeply_bnetd   (8B hdr + name\0 + difficulty)

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "app/d2cs/legacy_d2cs_bridges/send_authreply_bnetd_bridge.hpp"
#include "app/d2cs/legacy_d2cs_bridges/send_gameinforeply_bnetd_bridge.hpp"
#include "app/d2cs/legacy_d2cs_bridges/send_init_bnetd_bridge.hpp"
#include "app/d2cs/legacy_d2cs_bridges/send_packet_bridge.hpp"

namespace ild = pvpgn::integration::legacy_d2cs;

namespace {

struct FakeSink {
    static inline int call_count = 0;
    static inline void* last_conn = nullptr;
    static inline std::vector<unsigned char> last_bytes{};
    static void reset() noexcept {
        call_count = 0; last_conn = nullptr; last_bytes.clear();
    }
    static int handler(void* c, void const* b, unsigned int n) noexcept {
        ++call_count; last_conn = c;
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

}  // namespace

TEST_CASE("d2cs send_init_bnetd emits 1-byte init class 0x65",
          "[integration][legacy_d2cs][send_handle_bnetd_bridges]") {
    ScopedSink scope;
    int m = 0;
    REQUIRE(::pvpgn_v3_d2cs_send_init_bnetd(&m) == 1);
    REQUIRE(FakeSink::last_bytes.size() == 1u);
    REQUIRE(FakeSink::last_bytes[0] == 0x65);
    REQUIRE(FakeSink::last_conn == &m);
}

TEST_CASE("d2cs send_authreply_bnetd emits hdr + version + realm\\0",
          "[integration][legacy_d2cs][send_handle_bnetd_bridges]") {
    ScopedSink scope;
    int m = 0;
    REQUIRE(::pvpgn_v3_d2cs_send_authreply_bnetd(&m, 0x01u, 0x12345678u, "PvPGN") == 1);
    // 8 hdr + 4 version + 5 chars + 1 NUL = 18
    REQUIRE(FakeSink::last_bytes.size() == 18u);
    // size LE16 = 18
    REQUIRE(FakeSink::last_bytes[0] == 0x12);
    REQUIRE(FakeSink::last_bytes[1] == 0x00);
    // type LE16 = 0x0002
    REQUIRE(FakeSink::last_bytes[2] == 0x02);
    REQUIRE(FakeSink::last_bytes[3] == 0x00);
    // seqno LE32 = 1
    REQUIRE(FakeSink::last_bytes[4] == 0x01);
    REQUIRE(FakeSink::last_bytes[5] == 0x00);
    REQUIRE(FakeSink::last_bytes[6] == 0x00);
    REQUIRE(FakeSink::last_bytes[7] == 0x00);
    // version LE32
    REQUIRE(FakeSink::last_bytes[8]  == 0x78);
    REQUIRE(FakeSink::last_bytes[9]  == 0x56);
    REQUIRE(FakeSink::last_bytes[10] == 0x34);
    REQUIRE(FakeSink::last_bytes[11] == 0x12);
    // realm "PvPGN\0"
    REQUIRE(FakeSink::last_bytes[12] == 'P');
    REQUIRE(FakeSink::last_bytes[13] == 'v');
    REQUIRE(FakeSink::last_bytes[14] == 'P');
    REQUIRE(FakeSink::last_bytes[15] == 'G');
    REQUIRE(FakeSink::last_bytes[16] == 'N');
    REQUIRE(FakeSink::last_bytes[17] == 0x00);
}

TEST_CASE("d2cs send_gameinforeply_bnetd emits hdr + name\\0 + difficulty",
          "[integration][legacy_d2cs][send_handle_bnetd_bridges]") {
    ScopedSink scope;
    int m = 0;
    REQUIRE(::pvpgn_v3_d2cs_send_gameinforeply_bnetd(&m, 0x07u, "game1", 0x03u) == 1);
    // Wire: 8 hdr + 1 difficulty + 5 chars + 1 NUL = 15. Per
    // t_d2cs_bnetd_gameinforeply the difficulty byte precedes the gamename.
    REQUIRE(FakeSink::last_bytes.size() == 15u);
    REQUIRE(FakeSink::last_bytes[0] == 0x0f);
    REQUIRE(FakeSink::last_bytes[1] == 0x00);
    REQUIRE(FakeSink::last_bytes[2] == 0x12);
    REQUIRE(FakeSink::last_bytes[3] == 0x00);
    REQUIRE(FakeSink::last_bytes[4] == 0x07);
    REQUIRE(FakeSink::last_bytes[8] == 0x03);   // difficulty (offset 8)
    REQUIRE(FakeSink::last_bytes[9] == 'g');
    REQUIRE(FakeSink::last_bytes[10] == 'a');
    REQUIRE(FakeSink::last_bytes[11] == 'm');
    REQUIRE(FakeSink::last_bytes[12] == 'e');
    REQUIRE(FakeSink::last_bytes[13] == '1');
    REQUIRE(FakeSink::last_bytes[14] == 0x00);  // gamename NUL
}

TEST_CASE("d2cs handle_bnetd bridges reject null inputs",
          "[integration][legacy_d2cs][send_handle_bnetd_bridges]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_d2cs_send_init_bnetd(nullptr) == 0);
    REQUIRE(::pvpgn_v3_d2cs_send_authreply_bnetd(nullptr, 0, 0, "x") == 0);
    int m = 0;
    REQUIRE(::pvpgn_v3_d2cs_send_authreply_bnetd(&m, 0, 0, nullptr) == 0);
    REQUIRE(::pvpgn_v3_d2cs_send_gameinforeply_bnetd(nullptr, 0, "x", 0) == 0);
    REQUIRE(::pvpgn_v3_d2cs_send_gameinforeply_bnetd(&m, 0, nullptr, 0) == 0);
    REQUIRE(FakeSink::call_count == 0);
}
