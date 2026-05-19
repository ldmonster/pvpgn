// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_charlistreply`.
//
// Wire layout: header(4) + unknown1(4LE) + max_chars(4LE) + count(4LE) +
// raw char_data bytes (variable).
// Packet code: 0x37 (kSidCharList).

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_charlistreply_bridge.hpp"
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
    static int handler(void* conn_ptr, void const* bytes,
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

TEST_CASE("send_charlistreply declines with no handler installed",
          "[integration][legacy_bnetd][send_charlistreply_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_charlistreply(&marker, 0u, 8u, 0u, nullptr, 0u) == 0);
    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_charlistreply rejects null conn pointer",
          "[integration][legacy_bnetd][send_charlistreply_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_charlistreply(nullptr, 0u, 8u, 0u, nullptr, 0u) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_charlistreply empty list wire bytes",
          "[integration][legacy_bnetd][send_charlistreply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    // unknown1=0, max_chars=8, count=0, no char_data
    REQUIRE(::pvpgn_v3_send_charlistreply(&marker, 0u, 8u, 0u, nullptr, 0u) == 1);
    // header(4) + unknown1(4) + max_chars(4) + count(4) = 16 bytes
    // FF 37 10 00 | 00 00 00 00 | 08 00 00 00 | 00 00 00 00
    const unsigned char expected[] = {
        0xFF, 0x37, 0x10, 0x00,
        0x00, 0x00, 0x00, 0x00,  // unknown1 = 0
        0x08, 0x00, 0x00, 0x00,  // max_chars = 8
        0x00, 0x00, 0x00, 0x00   // count = 0
    };
    REQUIRE(FakeSink::last_bytes.size() == sizeof(expected));
    REQUIRE(std::memcmp(FakeSink::last_bytes.data(),
                        expected, sizeof(expected)) == 0);
}

TEST_CASE("send_charlistreply with char_data appended",
          "[integration][legacy_bnetd][send_charlistreply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    // Simulate 2 bytes of char_data
    const unsigned char data[] = {0xAB, 0xCD};
    REQUIRE(::pvpgn_v3_send_charlistreply(&marker, 1u, 8u, 1u, data, 2u) == 1);
    // header(4) + unknown1(4) + max_chars(4) + count(4) + 2 bytes = 18 bytes
    // FF 37 12 00 | 01 00 00 00 | 08 00 00 00 | 01 00 00 00 | AB CD
    const unsigned char expected[] = {
        0xFF, 0x37, 0x12, 0x00,
        0x01, 0x00, 0x00, 0x00,  // unknown1 = 1
        0x08, 0x00, 0x00, 0x00,  // max_chars = 8
        0x01, 0x00, 0x00, 0x00,  // count = 1
        0xAB, 0xCD
    };
    REQUIRE(FakeSink::last_bytes.size() == sizeof(expected));
    REQUIRE(std::memcmp(FakeSink::last_bytes.data(),
                        expected, sizeof(expected)) == 0);
}

TEST_CASE("send_charlistreply propagates handler return value",
          "[integration][legacy_bnetd][send_charlistreply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_charlistreply(&marker, 0u, 8u, 0u, nullptr, 0u) == -1);
}
