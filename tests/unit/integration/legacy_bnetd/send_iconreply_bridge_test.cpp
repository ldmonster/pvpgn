// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_iconreply`.
// Wire layout: header(4) + u64 timestamp (LE) + cstring filename.

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_iconreply_bridge.hpp"
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

TEST_CASE("send_iconreply declines with no handler installed",
          "[integration][legacy_bnetd][send_iconreply_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_iconreply(&marker, 0ull, "x") == 0);
    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_iconreply rejects null conn pointer",
          "[integration][legacy_bnetd][send_iconreply_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_iconreply(nullptr, 0ull, "x") == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_iconreply parity with legacy bytes (icons.bni)",
          "[integration][legacy_bnetd][send_iconreply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    // Reference (from bnet_protocol.h doc):
    // FF 2D 16 00 76 34 1F 8F C0 D6 BD 01 69 63 6F 6E 73 2E 62 6E 69 00
    // header(4) + 8-byte timestamp 0x01BDD6C08F1F3476 + "icons.bni\0"
    REQUIRE(::pvpgn_v3_send_iconreply(
        &marker, 0x01BDD6C08F1F3476ull, "icons.bni") == 1);
    const unsigned char expected[] = {
        0xFF, 0x2D, 0x16, 0x00,
        0x76, 0x34, 0x1F, 0x8F, 0xC0, 0xD6, 0xBD, 0x01,
        0x69, 0x63, 0x6F, 0x6E, 0x73, 0x2E, 0x62, 0x6E, 0x69, 0x00
    };
    REQUIRE(FakeSink::last_bytes.size() == sizeof(expected));
    REQUIRE(std::memcmp(FakeSink::last_bytes.data(),
                        expected, sizeof(expected)) == 0);
}

TEST_CASE("send_iconreply null filename emits empty NUL-terminated string",
          "[integration][legacy_bnetd][send_iconreply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_iconreply(&marker, 0ull, nullptr) == 1);
    // header(4) + 8 timestamp + 1 NUL = 13 bytes.
    REQUIRE(FakeSink::last_bytes.size() == 13u);
    REQUIRE(FakeSink::last_bytes[1] == 0x2Du);
    REQUIRE(FakeSink::last_bytes[2] == 0x0Du);
    for (std::size_t i = 4; i < 13; ++i) {
        REQUIRE(FakeSink::last_bytes[i] == 0x00u);
    }
}

TEST_CASE("send_iconreply propagates downstream handler return",
          "[integration][legacy_bnetd][send_iconreply_bridge]") {
    ScopedSink scope;
    FakeSink::return_value = -1;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_iconreply(&marker, 0ull, "x") == -1);
    REQUIRE(FakeSink::call_count == 1);
}
