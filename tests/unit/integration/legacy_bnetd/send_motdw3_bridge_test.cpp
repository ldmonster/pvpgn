// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_motdw3`.
//
// Verifies:
//   - null conn_ptr returns 0 without calling the handler.
//   - returns 0 when no send_packet handler is installed.
//   - emits correct wire bytes (header + fields + text cstring).
//   - null text treated as empty string.
//   - propagates handler return value (1 / 0 / -1).

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_motdw3_bridge.hpp"
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

TEST_CASE("send_motdw3 returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_motdw3_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);

    int marker = 0;
    REQUIRE(::pvpgn_v3_send_motdw3(&marker, 1u, 0u, 0u, 0u, 0u, nullptr) == 0);

    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_motdw3 rejects null conn pointer",
          "[integration][legacy_bnetd][send_motdw3_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_motdw3(nullptr, 1u, 0u, 0u, 0u, 0u, nullptr) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

// ---------------------------------------------------------------------------
// Wire byte correctness
// ---------------------------------------------------------------------------

TEST_CASE("send_motdw3 emits correct wire bytes (null text = empty)",
          "[integration][legacy_bnetd][send_motdw3_bridge]") {
    ScopedSink scope;
    int marker = 0;

    // msg_type=1, curr_time=0x10, first_news_time=0x20, timestamp=0x30,
    // timestamp2=0x40, text=nullptr
    REQUIRE(::pvpgn_v3_send_motdw3(&marker, 1u, 0x10u, 0x20u, 0x30u, 0x40u, nullptr) == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // header(4) + msg_type(1) + curr_time(4) + first_news_time(4) +
    // timestamp(4) + timestamp2(4) + "\0"(1) = 22 bytes
    REQUIRE(FakeSink::last_bytes.size() == 22u);

    // header: FF 1A 16 00  (SID_MOTD = 0x1A, size = 22 = 0x16)
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x1Au);
    REQUIRE(FakeSink::last_bytes[2] == 22u);
    REQUIRE(FakeSink::last_bytes[3] == 0u);

    // msg_type = 1
    REQUIRE(FakeSink::last_bytes[4] == 1u);

    // curr_time = 0x10 LE
    REQUIRE(FakeSink::last_bytes[5] == 0x10u);
    REQUIRE(FakeSink::last_bytes[6] == 0u);
    REQUIRE(FakeSink::last_bytes[7] == 0u);
    REQUIRE(FakeSink::last_bytes[8] == 0u);

    // first_news_time = 0x20 LE
    REQUIRE(FakeSink::last_bytes[9]  == 0x20u);
    REQUIRE(FakeSink::last_bytes[10] == 0u);
    REQUIRE(FakeSink::last_bytes[11] == 0u);
    REQUIRE(FakeSink::last_bytes[12] == 0u);

    // timestamp = 0x30 LE
    REQUIRE(FakeSink::last_bytes[13] == 0x30u);
    REQUIRE(FakeSink::last_bytes[14] == 0u);
    REQUIRE(FakeSink::last_bytes[15] == 0u);
    REQUIRE(FakeSink::last_bytes[16] == 0u);

    // timestamp2 = 0x40 LE
    REQUIRE(FakeSink::last_bytes[17] == 0x40u);
    REQUIRE(FakeSink::last_bytes[18] == 0u);
    REQUIRE(FakeSink::last_bytes[19] == 0u);
    REQUIRE(FakeSink::last_bytes[20] == 0u);

    // text = "\0"
    REQUIRE(FakeSink::last_bytes[21] == 0u);
}

TEST_CASE("send_motdw3 emits correct wire bytes with text",
          "[integration][legacy_bnetd][send_motdw3_bridge]") {
    ScopedSink scope;
    int marker = 0;

    REQUIRE(::pvpgn_v3_send_motdw3(&marker, 1u, 0u, 0u, 0u, 0u, "Hi") == 1);
    REQUIRE(FakeSink::call_count == 1);

    // header(4) + msg_type(1) + curr_time(4) + first_news_time(4) +
    // timestamp(4) + timestamp2(4) + "Hi\0"(3) = 24 bytes
    REQUIRE(FakeSink::last_bytes.size() == 24u);

    // "Hi\0" at offset 21
    REQUIRE(FakeSink::last_bytes[21] == 'H');
    REQUIRE(FakeSink::last_bytes[22] == 'i');
    REQUIRE(FakeSink::last_bytes[23] == 0u);
}

// ---------------------------------------------------------------------------
// Handler return propagation
// ---------------------------------------------------------------------------

TEST_CASE("send_motdw3 propagates handler return value",
          "[integration][legacy_bnetd][send_motdw3_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_motdw3(&marker, 1u, 0u, 0u, 0u, 0u, nullptr) == -1);

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_motdw3(&marker, 1u, 0u, 0u, 0u, 0u, nullptr) == 0);
}
