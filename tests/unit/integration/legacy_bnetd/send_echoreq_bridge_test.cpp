// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_echoreq`.
//
// Wire-format verification:
//   SERVER_ECHOREQ (SID_PING, 0x25): ff 25 08 00 + u32 LE = 8 bytes total
//
// Also verifies:
//   - null conn_ptr -> returns 0 (no crash, handler not called)
//   - returns 0 when no send_packet handler is installed
//   - propagates handler return values (1 / 0 / -1)

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_echoreq_bridge.hpp"
#include "integration/legacy_bnetd/send_packet_bridge.hpp"

namespace ila = pvpgn::integration::legacy_bnetd;

namespace {

struct FakeSink {
    static inline int     return_value     = 1;
    static inline int     call_count       = 0;
    static inline void*   last_conn        = nullptr;
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

TEST_CASE("send_echoreq returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_echoreq_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_echoreq(&marker, 0xdeadbeefu) == 0);
    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_echoreq rejects null conn pointer",
          "[integration][legacy_bnetd][send_echoreq_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_echoreq(nullptr, 0u) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_echoreq emits 8-byte SERVER_ECHOREQ wire format",
          "[integration][legacy_bnetd][send_echoreq_bridge]") {
    ScopedSink scope;
    int marker = 0;
    // ticks = 0x12345678
    REQUIRE(::pvpgn_v3_send_echoreq(&marker, 0x12345678u) == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // Total: header(4) + ticks(4) = 8 bytes
    REQUIRE(FakeSink::last_bytes.size() == 8u);

    // BNet header: ff SID=0x25 size=0x0008 (LE)
    REQUIRE(FakeSink::last_bytes[0] == 0xffu);
    REQUIRE(FakeSink::last_bytes[1] == 0x25u);
    REQUIRE(FakeSink::last_bytes[2] == 0x08u);
    REQUIRE(FakeSink::last_bytes[3] == 0x00u);

    // ticks = 0x12345678 LE
    REQUIRE(FakeSink::last_bytes[4] == 0x78u);
    REQUIRE(FakeSink::last_bytes[5] == 0x56u);
    REQUIRE(FakeSink::last_bytes[6] == 0x34u);
    REQUIRE(FakeSink::last_bytes[7] == 0x12u);
}

TEST_CASE("send_echoreq emits zero ticks correctly",
          "[integration][legacy_bnetd][send_echoreq_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_echoreq(&marker, 0u) == 1);
    REQUIRE(FakeSink::last_bytes.size() == 8u);
    REQUIRE(FakeSink::last_bytes[4] == 0x00u);
    REQUIRE(FakeSink::last_bytes[5] == 0x00u);
    REQUIRE(FakeSink::last_bytes[6] == 0x00u);
    REQUIRE(FakeSink::last_bytes[7] == 0x00u);
}

TEST_CASE("send_echoreq propagates handler return values",
          "[integration][legacy_bnetd][send_echoreq_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_echoreq(&marker, 0u) == 0);

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_echoreq(&marker, 0u) == -1);

    FakeSink::return_value = 1;
    REQUIRE(::pvpgn_v3_send_echoreq(&marker, 0u) == 1);
}
