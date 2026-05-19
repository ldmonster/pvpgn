// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_channellist`.
// Wire layout: header(4) + N NUL-terminated names + trailing NUL.

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_channellist_bridge.hpp"
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

TEST_CASE("send_channellist declines with no handler installed",
          "[integration][legacy_bnetd][send_channellist_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);
    int marker = 0;
    char const* n[] = {"Public Chat"};
    REQUIRE(::pvpgn_v3_send_channellist(&marker, n, 1) == 0);
    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_channellist rejects null conn pointer",
          "[integration][legacy_bnetd][send_channellist_bridge]") {
    ScopedSink scope;
    char const* n[] = {"Public Chat"};
    REQUIRE(::pvpgn_v3_send_channellist(nullptr, n, 1) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_channellist empty list emits just terminator",
          "[integration][legacy_bnetd][send_channellist_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_channellist(&marker, nullptr, 0) == 1);
    // header(4) + 1 NUL = 5 bytes: FF 0B 05 00 00
    const unsigned char expected[] = {
        0xFF, 0x0B, 0x05, 0x00, 0x00
    };
    REQUIRE(FakeSink::last_bytes.size() == sizeof(expected));
    REQUIRE(std::memcmp(FakeSink::last_bytes.data(),
                        expected, sizeof(expected)) == 0);
}

TEST_CASE("send_channellist three entries parity",
          "[integration][legacy_bnetd][send_channellist_bridge]") {
    ScopedSink scope;
    int marker = 0;
    char const* n[] = {"Public Chat", "Clan Chat", "W3"};
    REQUIRE(::pvpgn_v3_send_channellist(&marker, n, 3) == 1);
    // Expected: header(4) + "Public Chat\0" (12) + "Clan Chat\0" (10) +
    // "W3\0" (3) + "\0" (1) = 30 bytes. Size LE -> 0x1E 0x00.
    const unsigned char expected[] = {
        0xFF, 0x0B, 0x1E, 0x00,
        'P','u','b','l','i','c',' ','C','h','a','t', 0x00,
        'C','l','a','n',' ','C','h','a','t', 0x00,
        'W','3', 0x00,
        0x00
    };
    REQUIRE(FakeSink::last_bytes.size() == sizeof(expected));
    REQUIRE(std::memcmp(FakeSink::last_bytes.data(),
                        expected, sizeof(expected)) == 0);
}

TEST_CASE("send_channellist skips null and empty entries",
          "[integration][legacy_bnetd][send_channellist_bridge]") {
    ScopedSink scope;
    int marker = 0;
    char const* n[] = {"A", nullptr, "", "B"};
    REQUIRE(::pvpgn_v3_send_channellist(&marker, n, 4) == 1);
    // Expected: header(4) + "A\0" + "B\0" + "\0" = 9 bytes.
    const unsigned char expected[] = {
        0xFF, 0x0B, 0x09, 0x00,
        'A', 0x00,
        'B', 0x00,
        0x00
    };
    REQUIRE(FakeSink::last_bytes.size() == sizeof(expected));
    REQUIRE(std::memcmp(FakeSink::last_bytes.data(),
                        expected, sizeof(expected)) == 0);
}

TEST_CASE("send_channellist rejects oversize count",
          "[integration][legacy_bnetd][send_channellist_bridge]") {
    ScopedSink scope;
    int marker = 0;
    char const* dummy = "x";
    REQUIRE(::pvpgn_v3_send_channellist(&marker, &dummy, 2000u) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_channellist rejects non-zero count with null names",
          "[integration][legacy_bnetd][send_channellist_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_channellist(&marker, nullptr, 3) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_channellist propagates downstream handler return",
          "[integration][legacy_bnetd][send_channellist_bridge]") {
    ScopedSink scope;
    FakeSink::return_value = -1;
    int marker = 0;
    char const* n[] = {"x"};
    REQUIRE(::pvpgn_v3_send_channellist(&marker, n, 1) == -1);
    REQUIRE(FakeSink::call_count == 1);
}
