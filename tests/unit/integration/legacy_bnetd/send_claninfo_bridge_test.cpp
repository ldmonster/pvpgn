// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_claninforeply`.
//
// Verifies:
//   - null conn_ptr returns 0 without calling the handler.
//   - returns 0 when no send_packet handler is installed.
//   - emits correct wire bytes for failure reply (fail != 0).
//   - emits correct wire bytes for success reply (fail == 0).
//   - propagates handler return value (1 / 0 / -1).

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_claninfo_bridge.hpp"
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

TEST_CASE("send_claninforeply returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_claninfo_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);

    int marker = 0;
    REQUIRE(::pvpgn_v3_send_claninforeply(&marker, 0u, 1u, nullptr, 0u, 0u) == 0);

    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_claninforeply rejects null conn pointer",
          "[integration][legacy_bnetd][send_claninfo_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_claninforeply(nullptr, 0u, 1u, nullptr, 0u, 0u) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

// ---------------------------------------------------------------------------
// Wire byte correctness
// ---------------------------------------------------------------------------

TEST_CASE("send_claninforeply emits correct wire bytes for failure reply",
          "[integration][legacy_bnetd][send_claninfo_bridge]") {
    ScopedSink scope;
    int marker = 0;

    // cookie=0x11223344, fail=1 (failure path: no clan_name/rank/join_time)
    REQUIRE(::pvpgn_v3_send_claninforeply(&marker, 0x11223344u, 1u, nullptr, 0u, 0u) == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // header(4) + cookie(4) + fail(1) = 9 bytes
    REQUIRE(FakeSink::last_bytes.size() == 9u);

    // header: FF 82 09 00  (SID_CLANINFO = 0x82, size = 9)
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x82u);
    REQUIRE(FakeSink::last_bytes[2] == 9u);
    REQUIRE(FakeSink::last_bytes[3] == 0u);

    // cookie = 0x11223344 LE
    REQUIRE(FakeSink::last_bytes[4] == 0x44u);
    REQUIRE(FakeSink::last_bytes[5] == 0x33u);
    REQUIRE(FakeSink::last_bytes[6] == 0x22u);
    REQUIRE(FakeSink::last_bytes[7] == 0x11u);

    // fail = 1
    REQUIRE(FakeSink::last_bytes[8] == 1u);
}

TEST_CASE("send_claninforeply emits correct wire bytes for success reply",
          "[integration][legacy_bnetd][send_claninfo_bridge]") {
    ScopedSink scope;
    int marker = 0;

    // cookie=0, fail=0, clan_name="Clan", rank=3, join_time=0xABCD1234
    REQUIRE(::pvpgn_v3_send_claninforeply(&marker, 0u, 0u, "Clan", 3u, 0xABCD1234u) == 1);
    REQUIRE(FakeSink::call_count == 1);

    // header(4) + cookie(4) + fail(1) + "Clan\0"(5) + rank(1) + join_time(4) = 19 bytes
    REQUIRE(FakeSink::last_bytes.size() == 19u);

    // header: FF 82 13 00  (0x13 = 19)
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x82u);
    REQUIRE(FakeSink::last_bytes[2] == 19u);
    REQUIRE(FakeSink::last_bytes[3] == 0u);

    // cookie = 0 LE
    REQUIRE(FakeSink::last_bytes[4] == 0u);
    REQUIRE(FakeSink::last_bytes[5] == 0u);
    REQUIRE(FakeSink::last_bytes[6] == 0u);
    REQUIRE(FakeSink::last_bytes[7] == 0u);

    // fail = 0
    REQUIRE(FakeSink::last_bytes[8] == 0u);

    // "Clan\0" at offset 9
    REQUIRE(FakeSink::last_bytes[9]  == 'C');
    REQUIRE(FakeSink::last_bytes[10] == 'l');
    REQUIRE(FakeSink::last_bytes[11] == 'a');
    REQUIRE(FakeSink::last_bytes[12] == 'n');
    REQUIRE(FakeSink::last_bytes[13] == 0u);

    // rank = 3
    REQUIRE(FakeSink::last_bytes[14] == 3u);

    // join_time = 0xABCD1234 LE
    REQUIRE(FakeSink::last_bytes[15] == 0x34u);
    REQUIRE(FakeSink::last_bytes[16] == 0x12u);
    REQUIRE(FakeSink::last_bytes[17] == 0xCDu);
    REQUIRE(FakeSink::last_bytes[18] == 0xABu);
}

// ---------------------------------------------------------------------------
// Handler return propagation
// ---------------------------------------------------------------------------

TEST_CASE("send_claninforeply propagates handler return value",
          "[integration][legacy_bnetd][send_claninfo_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_claninforeply(&marker, 0u, 1u, nullptr, 0u, 0u) == -1);

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_claninforeply(&marker, 0u, 1u, nullptr, 0u, 0u) == 0);
}
