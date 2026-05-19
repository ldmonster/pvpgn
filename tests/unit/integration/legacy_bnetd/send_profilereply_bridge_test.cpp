// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_profilereply`.
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

#include "integration/legacy_bnetd/send_profilereply_bridge.hpp"
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

TEST_CASE("send_profilereply returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_profilereply_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);

    int marker = 0;
    REQUIRE(::pvpgn_v3_send_profilereply(&marker, 0u, 1u, nullptr, nullptr, 0u) == 0);

    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_profilereply rejects null conn pointer",
          "[integration][legacy_bnetd][send_profilereply_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_profilereply(nullptr, 0u, 1u, nullptr, nullptr, 0u) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

// ---------------------------------------------------------------------------
// Wire byte correctness
// ---------------------------------------------------------------------------

TEST_CASE("send_profilereply emits correct wire bytes for failure reply",
          "[integration][legacy_bnetd][send_profilereply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    // cookie=0xDEADBEEF, fail=1 (failure path: no description/location/clan_tag)
    REQUIRE(::pvpgn_v3_send_profilereply(&marker, 0xDEADBEEFu, 1u, nullptr, nullptr, 0u) == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // header(4) + cookie(4) + fail(1) = 9 bytes
    REQUIRE(FakeSink::last_bytes.size() == 9u);

    // header: FF 35 09 00  (SID_PROFILE = 0x35, size = 9)
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x35u);
    REQUIRE(FakeSink::last_bytes[2] == 9u);
    REQUIRE(FakeSink::last_bytes[3] == 0u);

    // cookie = 0xDEADBEEF LE
    REQUIRE(FakeSink::last_bytes[4] == 0xEFu);
    REQUIRE(FakeSink::last_bytes[5] == 0xBEu);
    REQUIRE(FakeSink::last_bytes[6] == 0xADu);
    REQUIRE(FakeSink::last_bytes[7] == 0xDEu);

    // fail = 1
    REQUIRE(FakeSink::last_bytes[8] == 1u);
}

TEST_CASE("send_profilereply emits correct wire bytes for success reply",
          "[integration][legacy_bnetd][send_profilereply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    // cookie=0, fail=0, description="Desc", location="Loc", clan_tag=0x57334E45
    REQUIRE(::pvpgn_v3_send_profilereply(&marker, 0u, 0u, "Desc", "Loc", 0x57334E45u) == 1);
    REQUIRE(FakeSink::call_count == 1);

    // header(4) + cookie(4) + fail(1) + "Desc\0"(5) + "Loc\0"(4) + clan_tag(4) = 22 bytes
    REQUIRE(FakeSink::last_bytes.size() == 22u);

    // header: FF 35 16 00  (0x16 = 22)
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x35u);
    REQUIRE(FakeSink::last_bytes[2] == 22u);
    REQUIRE(FakeSink::last_bytes[3] == 0u);

    // cookie = 0 LE
    REQUIRE(FakeSink::last_bytes[4] == 0u);
    REQUIRE(FakeSink::last_bytes[5] == 0u);
    REQUIRE(FakeSink::last_bytes[6] == 0u);
    REQUIRE(FakeSink::last_bytes[7] == 0u);

    // fail = 0
    REQUIRE(FakeSink::last_bytes[8] == 0u);

    // "Desc\0" at offset 9
    REQUIRE(FakeSink::last_bytes[9]  == 'D');
    REQUIRE(FakeSink::last_bytes[10] == 'e');
    REQUIRE(FakeSink::last_bytes[11] == 's');
    REQUIRE(FakeSink::last_bytes[12] == 'c');
    REQUIRE(FakeSink::last_bytes[13] == 0u);

    // "Loc\0" at offset 14
    REQUIRE(FakeSink::last_bytes[14] == 'L');
    REQUIRE(FakeSink::last_bytes[15] == 'o');
    REQUIRE(FakeSink::last_bytes[16] == 'c');
    REQUIRE(FakeSink::last_bytes[17] == 0u);

    // clan_tag = 0x57334E45 LE
    REQUIRE(FakeSink::last_bytes[18] == 0x45u);
    REQUIRE(FakeSink::last_bytes[19] == 0x4Eu);
    REQUIRE(FakeSink::last_bytes[20] == 0x33u);
    REQUIRE(FakeSink::last_bytes[21] == 0x57u);
}

// ---------------------------------------------------------------------------
// Handler return propagation
// ---------------------------------------------------------------------------

TEST_CASE("send_profilereply propagates handler return value",
          "[integration][legacy_bnetd][send_profilereply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_profilereply(&marker, 0u, 1u, nullptr, nullptr, 0u) == -1);

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_profilereply(&marker, 0u, 1u, nullptr, nullptr, 0u) == 0);
}
