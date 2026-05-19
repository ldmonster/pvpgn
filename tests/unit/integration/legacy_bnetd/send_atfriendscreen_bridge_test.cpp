// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_atfriendscreenreply`.
//
// Verifies:
//   - null conn_ptr returns 0 without calling the handler.
//   - returns 0 when no send_packet handler is installed.
//   - empty list emits correct wire bytes (header + count byte = 5 bytes).
//   - two-name list emits correct wire bytes.
//   - propagates handler return value (1 / 0 / -1).

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_atfriendscreen_bridge.hpp"
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

// ---------------------------------------------------------------------------
// Null / no-handler guard tests
// ---------------------------------------------------------------------------

TEST_CASE("send_atfriendscreenreply returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_atfriendscreen_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);

    int marker = 0;
    REQUIRE(::pvpgn_v3_send_atfriendscreenreply(&marker, nullptr, 0u) == 0);

    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_atfriendscreenreply rejects null conn pointer",
          "[integration][legacy_bnetd][send_atfriendscreen_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_atfriendscreenreply(nullptr, nullptr, 0u) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

// ---------------------------------------------------------------------------
// Empty list
// ---------------------------------------------------------------------------

TEST_CASE("send_atfriendscreenreply emits correct wire bytes for empty list",
          "[integration][legacy_bnetd][send_atfriendscreen_bridge]") {
    ScopedSink scope;
    int marker = 0;

    REQUIRE(::pvpgn_v3_send_atfriendscreenreply(&marker, nullptr, 0u) == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // header(4) + count(1) = 5 bytes
    REQUIRE(FakeSink::last_bytes.size() == 5u);

    // header: FF 60 05 00  (SID_ARRANGEDTEAM_FRIENDSCREEN = 0x60, size = 5)
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x60u);
    REQUIRE(FakeSink::last_bytes[2] == 5u);
    REQUIRE(FakeSink::last_bytes[3] == 0u);

    // count = 0
    REQUIRE(FakeSink::last_bytes[4] == 0u);
}

// ---------------------------------------------------------------------------
// Two-name list
// ---------------------------------------------------------------------------

TEST_CASE("send_atfriendscreenreply emits correct wire bytes for two names",
          "[integration][legacy_bnetd][send_atfriendscreen_bridge]") {
    ScopedSink scope;
    int marker = 0;

    char const* names[] = {"Bob", "Carol"};
    REQUIRE(::pvpgn_v3_send_atfriendscreenreply(&marker, names, 2u) == 1);
    REQUIRE(FakeSink::call_count == 1);

    // header(4) + count(1) + "Bob\0"(4) + "Carol\0"(6) = 15 bytes
    REQUIRE(FakeSink::last_bytes.size() == 15u);

    // header: FF 60 0F 00  (0x0F = 15)
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x60u);
    REQUIRE(FakeSink::last_bytes[2] == 15u);
    REQUIRE(FakeSink::last_bytes[3] == 0u);

    // count = 2
    REQUIRE(FakeSink::last_bytes[4] == 2u);

    // "Bob\0"
    REQUIRE(FakeSink::last_bytes[5] == 'B');
    REQUIRE(FakeSink::last_bytes[6] == 'o');
    REQUIRE(FakeSink::last_bytes[7] == 'b');
    REQUIRE(FakeSink::last_bytes[8] == 0u);

    // "Carol\0"
    REQUIRE(FakeSink::last_bytes[9]  == 'C');
    REQUIRE(FakeSink::last_bytes[10] == 'a');
    REQUIRE(FakeSink::last_bytes[11] == 'r');
    REQUIRE(FakeSink::last_bytes[12] == 'o');
    REQUIRE(FakeSink::last_bytes[13] == 'l');
    REQUIRE(FakeSink::last_bytes[14] == 0u);
}

// ---------------------------------------------------------------------------
// Handler return propagation
// ---------------------------------------------------------------------------

TEST_CASE("send_atfriendscreenreply propagates handler return value",
          "[integration][legacy_bnetd][send_atfriendscreen_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_atfriendscreenreply(&marker, nullptr, 0u) == -1);

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_atfriendscreenreply(&marker, nullptr, 0u) == 0);
}
