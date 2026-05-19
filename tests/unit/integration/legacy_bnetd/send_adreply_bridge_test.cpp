// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_adreply`.
//
// Wire layout: header(4) + adid(4LE) + extension_tag(4LE) + timestamp(8LE) +
// filename(cstring) + link(cstring).
// Packet code: 0x15 (kSidCheckAd).

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_adreply_bridge.hpp"
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

TEST_CASE("send_adreply declines with no handler installed",
          "[integration][legacy_bnetd][send_adreply_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_adreply(&marker, 1u, 0u, 0ull, "ad.smk", "http://x") == 0);
    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_adreply rejects null conn pointer",
          "[integration][legacy_bnetd][send_adreply_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_adreply(nullptr, 1u, 0u, 0ull, "ad.smk", "http://x") == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_adreply rejects null filename",
          "[integration][legacy_bnetd][send_adreply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_adreply(&marker, 1u, 0u, 0ull, nullptr, "http://x") == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_adreply rejects null link",
          "[integration][legacy_bnetd][send_adreply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_adreply(&marker, 1u, 0u, 0ull, "ad.smk", nullptr) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_adreply wire bytes parity",
          "[integration][legacy_bnetd][send_adreply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    // adid=1, extension_tag=0, timestamp=0, filename="a", link="b"
    REQUIRE(::pvpgn_v3_send_adreply(&marker, 1u, 0u, 0ull, "a", "b") == 1);
    // header(4) + adid(4) + ext_tag(4) + timestamp(8) + "a\0"(2) + "b\0"(2) = 24 bytes
    // FF 15 18 00 | 01 00 00 00 | 00 00 00 00 | 00 00 00 00 00 00 00 00 | 61 00 | 62 00
    const unsigned char expected[] = {
        0xFF, 0x15, 0x18, 0x00,
        0x01, 0x00, 0x00, 0x00,  // adid = 1
        0x00, 0x00, 0x00, 0x00,  // extension_tag = 0
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // timestamp = 0
        'a', 0x00,               // filename = "a"
        'b', 0x00                // link = "b"
    };
    REQUIRE(FakeSink::last_bytes.size() == sizeof(expected));
    REQUIRE(std::memcmp(FakeSink::last_bytes.data(),
                        expected, sizeof(expected)) == 0);
}

TEST_CASE("send_adreply propagates handler return value",
          "[integration][legacy_bnetd][send_adreply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_adreply(&marker, 1u, 0u, 0ull, "a", "b") == -1);
}
