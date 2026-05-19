// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_friendslistreply`.
//
// Verifies:
//   - null conn_ptr returns 0 without calling the handler.
//   - returns 0 when no send_packet handler is installed.
//   - empty list emits correct wire bytes (header + count byte = 5 bytes).
//   - single-entry list emits correct wire bytes.
//   - propagates handler return value (1 / 0 / -1).

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_friendslist_bridge.hpp"
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

TEST_CASE("send_friendslistreply returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_friendslist_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);

    int marker = 0;
    REQUIRE(::pvpgn_v3_send_friendslistreply(&marker, nullptr, 0u) == 0);

    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_friendslistreply rejects null conn pointer",
          "[integration][legacy_bnetd][send_friendslist_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_friendslistreply(nullptr, nullptr, 0u) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

// ---------------------------------------------------------------------------
// Empty list
// ---------------------------------------------------------------------------

TEST_CASE("send_friendslistreply emits correct wire bytes for empty list",
          "[integration][legacy_bnetd][send_friendslist_bridge]") {
    ScopedSink scope;
    int marker = 0;

    REQUIRE(::pvpgn_v3_send_friendslistreply(&marker, nullptr, 0u) == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // header(4) + count(1) = 5 bytes
    REQUIRE(FakeSink::last_bytes.size() == 5u);

    // header: FF 65 05 00  (SID_FRIENDSLIST = 0x65, size = 5)
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x65u);
    REQUIRE(FakeSink::last_bytes[2] == 5u);
    REQUIRE(FakeSink::last_bytes[3] == 0u);

    // count = 0
    REQUIRE(FakeSink::last_bytes[4] == 0u);
}

// ---------------------------------------------------------------------------
// Single-entry list
// ---------------------------------------------------------------------------

TEST_CASE("send_friendslistreply emits correct wire bytes for one entry",
          "[integration][legacy_bnetd][send_friendslist_bridge]") {
    ScopedSink scope;
    int marker = 0;

    pvpgn_v3_friend_entry entry{};
    entry.username      = "Alice";
    entry.status        = 0x01u;  // FRIEND_TYPE_MUTUAL
    entry.location      = 0x02u;  // FRIENDSTATUS_CHAT
    entry.client_tag    = 0x57335850u;  // 'W3XP'
    entry.location_name = "Ladder";

    REQUIRE(::pvpgn_v3_send_friendslistreply(&marker, &entry, 1u) == 1);
    REQUIRE(FakeSink::call_count == 1);

    // header(4) + count(1) + "Alice\0"(6) + status(1) + location(1) +
    // client_tag(4) + "Ladder\0"(7) = 24 bytes
    REQUIRE(FakeSink::last_bytes.size() == 24u);

    // header: FF 65 18 00  (0x18 = 24)
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x65u);
    REQUIRE(FakeSink::last_bytes[2] == 24u);
    REQUIRE(FakeSink::last_bytes[3] == 0u);

    // count = 1
    REQUIRE(FakeSink::last_bytes[4] == 1u);

    // "Alice\0"
    REQUIRE(FakeSink::last_bytes[5]  == 'A');
    REQUIRE(FakeSink::last_bytes[6]  == 'l');
    REQUIRE(FakeSink::last_bytes[7]  == 'i');
    REQUIRE(FakeSink::last_bytes[8]  == 'c');
    REQUIRE(FakeSink::last_bytes[9]  == 'e');
    REQUIRE(FakeSink::last_bytes[10] == 0u);

    // status = 0x01
    REQUIRE(FakeSink::last_bytes[11] == 0x01u);
    // location = 0x02
    REQUIRE(FakeSink::last_bytes[12] == 0x02u);

    // client_tag LE: 50 58 33 57 ('W3XP' stored as 0x57335850 LE)
    REQUIRE(FakeSink::last_bytes[13] == 0x50u);
    REQUIRE(FakeSink::last_bytes[14] == 0x58u);
    REQUIRE(FakeSink::last_bytes[15] == 0x33u);
    REQUIRE(FakeSink::last_bytes[16] == 0x57u);

    // "Ladder\0"
    REQUIRE(FakeSink::last_bytes[17] == 'L');
    REQUIRE(FakeSink::last_bytes[18] == 'a');
    REQUIRE(FakeSink::last_bytes[19] == 'd');
    REQUIRE(FakeSink::last_bytes[20] == 'd');
    REQUIRE(FakeSink::last_bytes[21] == 'e');
    REQUIRE(FakeSink::last_bytes[22] == 'r');
    REQUIRE(FakeSink::last_bytes[23] == 0u);
}

// ---------------------------------------------------------------------------
// Handler return propagation
// ---------------------------------------------------------------------------

TEST_CASE("send_friendslistreply propagates handler return value",
          "[integration][legacy_bnetd][send_friendslist_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_friendslistreply(&marker, nullptr, 0u) == -1);

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_friendslistreply(&marker, nullptr, 0u) == 0);
}
