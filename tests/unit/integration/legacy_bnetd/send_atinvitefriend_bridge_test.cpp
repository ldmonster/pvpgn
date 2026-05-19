// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_atinvitefriendack`.
//
// Verifies:
//   - null conn_ptr returns 0 without calling the handler.
//   - returns 0 when no send_packet handler is installed.
//   - emits correct wire bytes (header + count + id + timestamp + team_size +
//     5 × info u32).
//   - null info pointer treated as all-zeros.
//   - propagates handler return value (1 / 0 / -1).

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_atinvitefriend_bridge.hpp"
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

TEST_CASE("send_atinvitefriendack returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_atinvitefriend_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);

    int marker = 0;
    REQUIRE(::pvpgn_v3_send_atinvitefriendack(&marker, 0u, 0u, 0u, 0u, nullptr) == 0);

    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_atinvitefriendack rejects null conn pointer",
          "[integration][legacy_bnetd][send_atinvitefriend_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_atinvitefriendack(nullptr, 0u, 0u, 0u, 0u, nullptr) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

// ---------------------------------------------------------------------------
// Wire byte correctness
// ---------------------------------------------------------------------------

TEST_CASE("send_atinvitefriendack emits correct wire bytes (null info = all-zeros)",
          "[integration][legacy_bnetd][send_atinvitefriend_bridge]") {
    ScopedSink scope;
    int marker = 0;

    // count=1, id=2, timestamp=0x12345678, team_size=2, info=nullptr
    REQUIRE(::pvpgn_v3_send_atinvitefriendack(
                &marker, 1u, 2u, 0x12345678u, 2u, nullptr) == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // header(4) + count(4) + id(4) + timestamp(4) + team_size(1) +
    // info[5](20) = 37 bytes
    REQUIRE(FakeSink::last_bytes.size() == 37u);

    // header: FF 61 25 00  (SID_ARRANGEDTEAM_INVITE_FRIEND = 0x61, size = 37)
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x61u);
    REQUIRE(FakeSink::last_bytes[2] == 37u);
    REQUIRE(FakeSink::last_bytes[3] == 0u);

    // count = 1 LE
    REQUIRE(FakeSink::last_bytes[4] == 1u);
    REQUIRE(FakeSink::last_bytes[5] == 0u);
    REQUIRE(FakeSink::last_bytes[6] == 0u);
    REQUIRE(FakeSink::last_bytes[7] == 0u);

    // id = 2 LE
    REQUIRE(FakeSink::last_bytes[8]  == 2u);
    REQUIRE(FakeSink::last_bytes[9]  == 0u);
    REQUIRE(FakeSink::last_bytes[10] == 0u);
    REQUIRE(FakeSink::last_bytes[11] == 0u);

    // timestamp = 0x12345678 LE
    REQUIRE(FakeSink::last_bytes[12] == 0x78u);
    REQUIRE(FakeSink::last_bytes[13] == 0x56u);
    REQUIRE(FakeSink::last_bytes[14] == 0x34u);
    REQUIRE(FakeSink::last_bytes[15] == 0x12u);

    // team_size = 2
    REQUIRE(FakeSink::last_bytes[16] == 2u);

    // info[0..4] = all zeros (null pointer)
    for (int i = 0; i < 20; ++i) {
        REQUIRE(FakeSink::last_bytes[17 + i] == 0u);
    }
}

TEST_CASE("send_atinvitefriendack emits correct info array",
          "[integration][legacy_bnetd][send_atinvitefriend_bridge]") {
    ScopedSink scope;
    int marker = 0;

    unsigned int info[5] = {0x00000001u, 0x00000002u,
                             0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};
    REQUIRE(::pvpgn_v3_send_atinvitefriendack(
                &marker, 0u, 0u, 0u, 2u, info) == 1);

    REQUIRE(FakeSink::last_bytes.size() == 37u);

    // info[0] = 0x00000001 LE
    REQUIRE(FakeSink::last_bytes[17] == 0x01u);
    REQUIRE(FakeSink::last_bytes[18] == 0x00u);
    REQUIRE(FakeSink::last_bytes[19] == 0x00u);
    REQUIRE(FakeSink::last_bytes[20] == 0x00u);

    // info[1] = 0x00000002 LE
    REQUIRE(FakeSink::last_bytes[21] == 0x02u);
    REQUIRE(FakeSink::last_bytes[22] == 0x00u);
    REQUIRE(FakeSink::last_bytes[23] == 0x00u);
    REQUIRE(FakeSink::last_bytes[24] == 0x00u);

    // info[2..4] = 0xFFFFFFFF LE
    for (int i = 0; i < 3; ++i) {
        REQUIRE(FakeSink::last_bytes[25 + i * 4 + 0] == 0xFFu);
        REQUIRE(FakeSink::last_bytes[25 + i * 4 + 1] == 0xFFu);
        REQUIRE(FakeSink::last_bytes[25 + i * 4 + 2] == 0xFFu);
        REQUIRE(FakeSink::last_bytes[25 + i * 4 + 3] == 0xFFu);
    }
}

// ---------------------------------------------------------------------------
// Handler return propagation
// ---------------------------------------------------------------------------

TEST_CASE("send_atinvitefriendack propagates handler return value",
          "[integration][legacy_bnetd][send_atinvitefriend_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_atinvitefriendack(&marker, 0u, 0u, 0u, 0u, nullptr) == -1);

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_atinvitefriendack(&marker, 0u, 0u, 0u, 0u, nullptr) == 0);
}
