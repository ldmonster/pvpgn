// SPDX-License-Identifier: GPL-2.0-or-later
// Wire-format tests for the SERVER_FRIEND{ADD,DEL,MOVE}_ACK bridges.

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_friendadd_ack_bridge.hpp"
#include "integration/legacy_bnetd/send_frienddel_ack_bridge.hpp"
#include "integration/legacy_bnetd/send_friendmove_ack_bridge.hpp"
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

TEST_CASE("send_frienddel_ack encodes 5-byte wire (hdr+u8)",
          "[integration][legacy_bnetd][send_friend_acks]") {
    ScopedSink scope;
    int m = 0;
    REQUIRE(::pvpgn_v3_send_frienddel_ack(&m, 0x42u) == 1);
    REQUIRE(FakeSink::last_bytes.size() == 5u);
    REQUIRE(FakeSink::last_bytes[0] == 0xff);
    REQUIRE(FakeSink::last_bytes[1] == 0x68);
    REQUIRE(FakeSink::last_bytes[2] == 0x05);
    REQUIRE(FakeSink::last_bytes[3] == 0x00);
    REQUIRE(FakeSink::last_bytes[4] == 0x42);
}

TEST_CASE("send_friendmove_ack encodes 6-byte wire (hdr+u8+u8)",
          "[integration][legacy_bnetd][send_friend_acks]") {
    ScopedSink scope;
    int m = 0;
    REQUIRE(::pvpgn_v3_send_friendmove_ack(&m, 0x03u, 0x04u) == 1);
    REQUIRE(FakeSink::last_bytes.size() == 6u);
    REQUIRE(FakeSink::last_bytes[0] == 0xff);
    REQUIRE(FakeSink::last_bytes[1] == 0x69);
    REQUIRE(FakeSink::last_bytes[2] == 0x06);
    REQUIRE(FakeSink::last_bytes[3] == 0x00);
    REQUIRE(FakeSink::last_bytes[4] == 0x03);
    REQUIRE(FakeSink::last_bytes[5] == 0x04);
}

TEST_CASE("send_friendadd_ack encodes name + status + location + tag + loc_name",
          "[integration][legacy_bnetd][send_friend_acks]") {
    ScopedSink scope;
    int m = 0;
    // name="foo" (4B w/ NUL), status=0x01, location=0x02,
    // client_tag = 'PX2D' little-endian = 0x44325850 packed, but treat as u32.
    REQUIRE(::pvpgn_v3_send_friendadd_ack(&m, "foo",
                0x01u, 0x02u, 0x12345678u, "ch") == 1);
    // size = 4 (hdr) + 4 (name "foo\0") + 1 + 1 + 4 + 3 (loc_name "ch\0") = 17
    REQUIRE(FakeSink::last_bytes.size() == 17u);
    REQUIRE(FakeSink::last_bytes[0] == 0xff);
    REQUIRE(FakeSink::last_bytes[1] == 0x67);
    REQUIRE(FakeSink::last_bytes[2] == 0x11);   // 17 LE
    REQUIRE(FakeSink::last_bytes[3] == 0x00);
    // name "foo\0"
    REQUIRE(FakeSink::last_bytes[4] == 'f');
    REQUIRE(FakeSink::last_bytes[5] == 'o');
    REQUIRE(FakeSink::last_bytes[6] == 'o');
    REQUIRE(FakeSink::last_bytes[7] == 0x00);
    REQUIRE(FakeSink::last_bytes[8] == 0x01);   // status
    REQUIRE(FakeSink::last_bytes[9] == 0x02);   // location
    // client_tag 0x12345678 LE
    REQUIRE(FakeSink::last_bytes[10] == 0x78);
    REQUIRE(FakeSink::last_bytes[11] == 0x56);
    REQUIRE(FakeSink::last_bytes[12] == 0x34);
    REQUIRE(FakeSink::last_bytes[13] == 0x12);
    REQUIRE(FakeSink::last_bytes[14] == 'c');
    REQUIRE(FakeSink::last_bytes[15] == 'h');
    REQUIRE(FakeSink::last_bytes[16] == 0x00);
}

TEST_CASE("friend ack bridges reject null conn",
          "[integration][legacy_bnetd][send_friend_acks]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_frienddel_ack(nullptr, 0u) == 0);
    REQUIRE(::pvpgn_v3_send_friendmove_ack(nullptr, 0u, 1u) == 0);
    REQUIRE(::pvpgn_v3_send_friendadd_ack(nullptr, "x", 0, 0, 0, "y") == 0);
    int m = 0;
    REQUIRE(::pvpgn_v3_send_friendadd_ack(&m, nullptr, 0, 0, 0, "y") == 0);
    REQUIRE(::pvpgn_v3_send_friendadd_ack(&m, "x", 0, 0, 0, nullptr) == 0);
    REQUIRE(FakeSink::call_count == 0);
}
