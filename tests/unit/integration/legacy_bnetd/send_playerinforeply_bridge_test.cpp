// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_playerinforeply`.
//
// Wire layout: header(4) + account_name(cstring) + player_info(cstring) +
// username(cstring).
// Packet code: 0x0A (SERVER_PLAYERINFOREPLY / SID_USERDATA).

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_playerinforeply_bridge.hpp"
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

TEST_CASE("send_playerinforeply declines with no handler installed",
          "[integration][legacy_bnetd][send_playerinforeply_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_playerinforeply(&marker, "acc", "info", "user") == 0);
    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_playerinforeply rejects null conn pointer",
          "[integration][legacy_bnetd][send_playerinforeply_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_playerinforeply(nullptr, "acc", "info", "user") == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_playerinforeply rejects null account_name",
          "[integration][legacy_bnetd][send_playerinforeply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_playerinforeply(&marker, nullptr, "info", "user") == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_playerinforeply rejects null player_info",
          "[integration][legacy_bnetd][send_playerinforeply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_playerinforeply(&marker, "acc", nullptr, "user") == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_playerinforeply rejects null username",
          "[integration][legacy_bnetd][send_playerinforeply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_playerinforeply(&marker, "acc", "info", nullptr) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_playerinforeply wire bytes parity",
          "[integration][legacy_bnetd][send_playerinforeply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    // account_name="a", player_info="b", username="c"
    REQUIRE(::pvpgn_v3_send_playerinforeply(&marker, "a", "b", "c") == 1);
    // header(4) + "a\0"(2) + "b\0"(2) + "c\0"(2) = 10 bytes
    // FF 0A 0A 00 | 61 00 | 62 00 | 63 00
    const unsigned char expected[] = {
        0xFF, 0x0A, 0x0A, 0x00,
        'a',  0x00,              // account_name = "a"
        'b',  0x00,              // player_info = "b"
        'c',  0x00               // username = "c"
    };
    REQUIRE(FakeSink::last_bytes.size() == sizeof(expected));
    REQUIRE(std::memcmp(FakeSink::last_bytes.data(),
                        expected, sizeof(expected)) == 0);
}

TEST_CASE("send_playerinforeply propagates handler return value",
          "[integration][legacy_bnetd][send_playerinforeply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_playerinforeply(&marker, "a", "b", "c") == -1);
}
