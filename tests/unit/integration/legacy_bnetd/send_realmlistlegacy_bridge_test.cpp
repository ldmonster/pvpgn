// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_realmlistlegacyreply`.
//
// Verifies:
//   - null conn_ptr returns 0 without calling the handler.
//   - returns 0 when no send_packet handler is installed.
//   - emits correct wire bytes for empty realm list.
//   - emits correct wire bytes for a single legacy realm entry.
//   - propagates handler return value (1 / 0 / -1).

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_realmlistlegacy_bridge.hpp"
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

TEST_CASE("send_realmlistlegacyreply returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_realmlistlegacy_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);

    int marker = 0;
    REQUIRE(::pvpgn_v3_send_realmlistlegacyreply(&marker, nullptr, 0u) == 0);

    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_realmlistlegacyreply rejects null conn pointer",
          "[integration][legacy_bnetd][send_realmlistlegacy_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_realmlistlegacyreply(nullptr, nullptr, 0u) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

// ---------------------------------------------------------------------------
// Wire byte correctness
// ---------------------------------------------------------------------------

TEST_CASE("send_realmlistlegacyreply emits correct wire bytes for empty list",
          "[integration][legacy_bnetd][send_realmlistlegacy_bridge]") {
    ScopedSink scope;
    int marker = 0;

    REQUIRE(::pvpgn_v3_send_realmlistlegacyreply(&marker, nullptr, 0u) == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // header(4) + unknown1(4) + count(4) = 12 bytes
    REQUIRE(FakeSink::last_bytes.size() == 12u);

    // header: FF 34 0C 00  (SID_REALMLISTLEGACY = 0x34, size = 12)
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x34u);
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

TEST_CASE("send_realmlistlegacyreply emits correct wire bytes for one entry",
          "[integration][legacy_bnetd][send_realmlistlegacy_bridge]") {
    ScopedSink scope;
    int marker = 0;

    pvpgn_v3_realm_legacy_entry entry;
    entry.unknown3    = 0xC0000000u;
    entry.unknown4    = 0u;
    entry.unknown5    = 0u;
    entry.unknown6    = 0u;
    entry.unknown7    = 0x00018210u;
    entry.unknown8    = 0xFFFFFFFFu;
    entry.unknown9    = 0u;
    entry.name        = "US East";
    entry.description = "East";

    REQUIRE(::pvpgn_v3_send_realmlistlegacyreply(&marker, &entry, 1u) == 1);
    REQUIRE(FakeSink::call_count == 1);

    // header(4) + unknown1(4) + count(4) +
    // 7x u32(28) + "US East\0"(8) + "East\0"(5) = 53 bytes
    REQUIRE(FakeSink::last_bytes.size() == 53u);

    // header: FF 34 35 00  (0x35 = 53)
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x34u);
    REQUIRE(FakeSink::last_bytes[2] == 53u);
    REQUIRE(FakeSink::last_bytes[3] == 0u);

    // count = 1 LE at offset 8
    REQUIRE(FakeSink::last_bytes[8]  == 1u);
    REQUIRE(FakeSink::last_bytes[9]  == 0u);
    REQUIRE(FakeSink::last_bytes[10] == 0u);
    REQUIRE(FakeSink::last_bytes[11] == 0u);

    // unknown3 = 0xC0000000 LE at offset 12
    REQUIRE(FakeSink::last_bytes[12] == 0x00u);
    REQUIRE(FakeSink::last_bytes[13] == 0x00u);
    REQUIRE(FakeSink::last_bytes[14] == 0x00u);
    REQUIRE(FakeSink::last_bytes[15] == 0xC0u);

    // unknown7 = 0x00018210 LE at offset 28
    REQUIRE(FakeSink::last_bytes[28] == 0x10u);
    REQUIRE(FakeSink::last_bytes[29] == 0x82u);
    REQUIRE(FakeSink::last_bytes[30] == 0x01u);
    REQUIRE(FakeSink::last_bytes[31] == 0x00u);

    // unknown8 = 0xFFFFFFFF LE at offset 32
    REQUIRE(FakeSink::last_bytes[32] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[33] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[34] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[35] == 0xFFu);

    // "US East\0" at offset 40
    REQUIRE(FakeSink::last_bytes[40] == 'U');
    REQUIRE(FakeSink::last_bytes[41] == 'S');
    REQUIRE(FakeSink::last_bytes[42] == ' ');
    REQUIRE(FakeSink::last_bytes[43] == 'E');
    REQUIRE(FakeSink::last_bytes[44] == 'a');
    REQUIRE(FakeSink::last_bytes[45] == 's');
    REQUIRE(FakeSink::last_bytes[46] == 't');
    REQUIRE(FakeSink::last_bytes[47] == 0u);

    // "East\0" at offset 48
    REQUIRE(FakeSink::last_bytes[48] == 'E');
    REQUIRE(FakeSink::last_bytes[49] == 'a');
    REQUIRE(FakeSink::last_bytes[50] == 's');
    REQUIRE(FakeSink::last_bytes[51] == 't');
    REQUIRE(FakeSink::last_bytes[52] == 0u);
}

// ---------------------------------------------------------------------------
// Handler return propagation
// ---------------------------------------------------------------------------

TEST_CASE("send_realmlistlegacyreply propagates handler return value",
          "[integration][legacy_bnetd][send_realmlistlegacy_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_realmlistlegacyreply(&marker, nullptr, 0u) == -1);

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_realmlistlegacyreply(&marker, nullptr, 0u) == 0);
}
