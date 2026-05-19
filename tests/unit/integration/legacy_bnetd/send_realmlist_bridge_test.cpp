// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_realmlistreply`.
//
// Verifies:
//   - null conn_ptr returns 0 without calling the handler.
//   - returns 0 when no send_packet handler is installed.
//   - emits correct wire bytes for empty realm list.
//   - emits correct wire bytes for a single realm entry.
//   - propagates handler return value (1 / 0 / -1).

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_realmlist_bridge.hpp"
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

TEST_CASE("send_realmlistreply returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_realmlist_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);

    int marker = 0;
    REQUIRE(::pvpgn_v3_send_realmlistreply(&marker, nullptr, 0u) == 0);

    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_realmlistreply rejects null conn pointer",
          "[integration][legacy_bnetd][send_realmlist_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_realmlistreply(nullptr, nullptr, 0u) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

// ---------------------------------------------------------------------------
// Wire byte correctness
// ---------------------------------------------------------------------------

TEST_CASE("send_realmlistreply emits correct wire bytes for empty list",
          "[integration][legacy_bnetd][send_realmlist_bridge]") {
    ScopedSink scope;
    int marker = 0;

    REQUIRE(::pvpgn_v3_send_realmlistreply(&marker, nullptr, 0u) == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // header(4) + unknown1(4) + count(4) = 12 bytes
    REQUIRE(FakeSink::last_bytes.size() == 12u);

    // header: FF 40 0C 00  (SID_REALMLIST = 0x40, size = 12)
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x40u);
    REQUIRE(FakeSink::last_bytes[2] == 12u);
    REQUIRE(FakeSink::last_bytes[3] == 0u);

    // unknown1 = 0 LE
    REQUIRE(FakeSink::last_bytes[4] == 0u);
    REQUIRE(FakeSink::last_bytes[5] == 0u);
    REQUIRE(FakeSink::last_bytes[6] == 0u);
    REQUIRE(FakeSink::last_bytes[7] == 0u);

    // count = 0 LE
    REQUIRE(FakeSink::last_bytes[8]  == 0u);
    REQUIRE(FakeSink::last_bytes[9]  == 0u);
    REQUIRE(FakeSink::last_bytes[10] == 0u);
    REQUIRE(FakeSink::last_bytes[11] == 0u);
}

TEST_CASE("send_realmlistreply emits correct wire bytes for one entry",
          "[integration][legacy_bnetd][send_realmlist_bridge]") {
    ScopedSink scope;
    int marker = 0;

    pvpgn_v3_realm_entry entry;
    entry.unknown     = 1u;
    entry.name        = "US East";
    entry.description = "East";

    REQUIRE(::pvpgn_v3_send_realmlistreply(&marker, &entry, 1u) == 1);
    REQUIRE(FakeSink::call_count == 1);

    // header(4) + unknown1(4) + count(4) + unknown(4) + "US East\0"(8) + "East\0"(5) = 29 bytes
    REQUIRE(FakeSink::last_bytes.size() == 29u);

    // header: FF 40 1D 00  (0x1D = 29)
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x40u);
    REQUIRE(FakeSink::last_bytes[2] == 29u);
    REQUIRE(FakeSink::last_bytes[3] == 0u);

    // count = 1 LE
    REQUIRE(FakeSink::last_bytes[8]  == 1u);
    REQUIRE(FakeSink::last_bytes[9]  == 0u);
    REQUIRE(FakeSink::last_bytes[10] == 0u);
    REQUIRE(FakeSink::last_bytes[11] == 0u);

    // entry.unknown = 1 LE
    REQUIRE(FakeSink::last_bytes[12] == 1u);
    REQUIRE(FakeSink::last_bytes[13] == 0u);
    REQUIRE(FakeSink::last_bytes[14] == 0u);
    REQUIRE(FakeSink::last_bytes[15] == 0u);

    // "US East\0"
    REQUIRE(FakeSink::last_bytes[16] == 'U');
    REQUIRE(FakeSink::last_bytes[17] == 'S');
    REQUIRE(FakeSink::last_bytes[18] == ' ');
    REQUIRE(FakeSink::last_bytes[19] == 'E');
    REQUIRE(FakeSink::last_bytes[20] == 'a');
    REQUIRE(FakeSink::last_bytes[21] == 's');
    REQUIRE(FakeSink::last_bytes[22] == 't');
    REQUIRE(FakeSink::last_bytes[23] == 0u);

    // "East\0"
    REQUIRE(FakeSink::last_bytes[24] == 'E');
    REQUIRE(FakeSink::last_bytes[25] == 'a');
    REQUIRE(FakeSink::last_bytes[26] == 's');
    REQUIRE(FakeSink::last_bytes[27] == 't');
    REQUIRE(FakeSink::last_bytes[28] == 0u);
}

// ---------------------------------------------------------------------------
// Handler return propagation
// ---------------------------------------------------------------------------

TEST_CASE("send_realmlistreply propagates handler return value",
          "[integration][legacy_bnetd][send_realmlist_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_realmlistreply(&marker, nullptr, 0u) == -1);

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_realmlistreply(&marker, nullptr, 0u) == 0);
}
