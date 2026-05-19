// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_adclick2reply`.
//
// Wire layout: header(4) + adid(4LE) + link(cstring).
// Packet code: 0x41 (kSidAdClick2).

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_adclick2reply_bridge.hpp"
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

TEST_CASE("send_adclick2reply declines with no handler installed",
          "[integration][legacy_bnetd][send_adclick2reply_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_adclick2reply(&marker, 1u, "http://x") == 0);
    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_adclick2reply rejects null conn pointer",
          "[integration][legacy_bnetd][send_adclick2reply_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_adclick2reply(nullptr, 1u, "http://x") == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_adclick2reply rejects null link",
          "[integration][legacy_bnetd][send_adclick2reply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_adclick2reply(&marker, 1u, nullptr) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_adclick2reply wire bytes parity",
          "[integration][legacy_bnetd][send_adclick2reply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    // adid=1, link="b"
    REQUIRE(::pvpgn_v3_send_adclick2reply(&marker, 1u, "b") == 1);
    // header(4) + adid(4) + "b\0"(2) = 10 bytes
    // FF 41 0A 00 | 01 00 00 00 | 62 00
    const unsigned char expected[] = {
        0xFF, 0x41, 0x0A, 0x00,
        0x01, 0x00, 0x00, 0x00,  // adid = 1
        'b',  0x00               // link = "b"
    };
    REQUIRE(FakeSink::last_bytes.size() == sizeof(expected));
    REQUIRE(std::memcmp(FakeSink::last_bytes.data(),
                        expected, sizeof(expected)) == 0);
}

TEST_CASE("send_adclick2reply propagates handler return value",
          "[integration][legacy_bnetd][send_adclick2reply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_adclick2reply(&marker, 1u, "b") == -1);
}
