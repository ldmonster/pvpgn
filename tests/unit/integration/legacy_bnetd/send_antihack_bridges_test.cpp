// SPDX-License-Identifier: GPL-2.0-or-later
// Wire-format tests for SERVER_READMEMORY (0x17) and SERVER_REQUIREDWORK (0x4C) bridges.

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_readmemory_bridge.hpp"
#include "integration/legacy_bnetd/send_requiredwork_bridge.hpp"
#include "integration/legacy_bnetd/send_packet_bridge.hpp"

namespace ila = pvpgn::integration::legacy_bnetd;

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
    ila::SendPacketHandler prev = ila::get_send_packet_handler();
    ScopedSink() noexcept { FakeSink::reset(); ila::set_send_packet_handler(&FakeSink::handler); }
    ~ScopedSink() noexcept { ila::set_send_packet_handler(prev); }
};

}  // namespace

TEST_CASE("send_readmemory encodes 16-byte wire (hdr + 3 u32)",
          "[integration][legacy_bnetd][send_antihack_bridges]") {
    ScopedSink scope;
    int m = 0;
    REQUIRE(::pvpgn_v3_send_readmemory(&m, 0x01020304u, 0x05060708u, 0x090a0b0cu) == 1);
    REQUIRE(FakeSink::last_bytes.size() == 16u);
    REQUIRE(FakeSink::last_bytes[0] == 0xff);
    REQUIRE(FakeSink::last_bytes[1] == 0x17);
    REQUIRE(FakeSink::last_bytes[2] == 0x10);   // 16 LE
    REQUIRE(FakeSink::last_bytes[3] == 0x00);
    // request_id LE
    REQUIRE(FakeSink::last_bytes[4] == 0x04);
    REQUIRE(FakeSink::last_bytes[5] == 0x03);
    REQUIRE(FakeSink::last_bytes[6] == 0x02);
    REQUIRE(FakeSink::last_bytes[7] == 0x01);
    // address LE
    REQUIRE(FakeSink::last_bytes[8]  == 0x08);
    REQUIRE(FakeSink::last_bytes[9]  == 0x07);
    REQUIRE(FakeSink::last_bytes[10] == 0x06);
    REQUIRE(FakeSink::last_bytes[11] == 0x05);
    // length LE
    REQUIRE(FakeSink::last_bytes[12] == 0x0c);
    REQUIRE(FakeSink::last_bytes[13] == 0x0b);
    REQUIRE(FakeSink::last_bytes[14] == 0x0a);
    REQUIRE(FakeSink::last_bytes[15] == 0x09);
}

TEST_CASE("send_requiredwork encodes hdr + filename cstring",
          "[integration][legacy_bnetd][send_antihack_bridges]") {
    ScopedSink scope;
    int m = 0;
    REQUIRE(::pvpgn_v3_send_requiredwork(&m, "IX86ExtraWork.mpq") == 1);
    // hdr(4) + 17 chars + 1 NUL = 22
    REQUIRE(FakeSink::last_bytes.size() == 22u);
    REQUIRE(FakeSink::last_bytes[0] == 0xff);
    REQUIRE(FakeSink::last_bytes[1] == 0x4c);
    REQUIRE(FakeSink::last_bytes[2] == 0x16);   // 22 LE
    REQUIRE(FakeSink::last_bytes[3] == 0x00);
    char const* expected = "IX86ExtraWork.mpq";
    for (std::size_t i = 0; i < 17; ++i) {
        REQUIRE(FakeSink::last_bytes[4 + i] == static_cast<unsigned char>(expected[i]));
    }
    REQUIRE(FakeSink::last_bytes[21] == 0x00);
}

TEST_CASE("antihack bridges reject null inputs",
          "[integration][legacy_bnetd][send_antihack_bridges]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_readmemory(nullptr, 0, 0, 0) == 0);
    REQUIRE(::pvpgn_v3_send_requiredwork(nullptr, "x") == 0);
    int m = 0;
    REQUIRE(::pvpgn_v3_send_requiredwork(&m, nullptr) == 0);
    REQUIRE(FakeSink::call_count == 0);
}
