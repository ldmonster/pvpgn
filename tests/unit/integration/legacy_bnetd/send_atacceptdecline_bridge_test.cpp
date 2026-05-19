// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_atmemberdecline`.
//
// Verifies:
//   - null conn_ptr returns 0 without calling the handler.
//   - returns 0 when no send_packet handler is installed.
//   - emits correct wire bytes (header + count + action + decliner_name cstring).
//   - null decliner_name treated as empty string.
//   - propagates handler return value (1 / 0 / -1).

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_atacceptdecline_bridge.hpp"
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

TEST_CASE("send_atmemberdecline returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_atacceptdecline_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);

    int marker = 0;
    REQUIRE(::pvpgn_v3_send_atmemberdecline(&marker, 0u, 0u, nullptr) == 0);

    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_atmemberdecline rejects null conn pointer",
          "[integration][legacy_bnetd][send_atacceptdecline_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_atmemberdecline(nullptr, 0u, 0u, nullptr) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

// ---------------------------------------------------------------------------
// Wire byte correctness
// ---------------------------------------------------------------------------

TEST_CASE("send_atmemberdecline emits correct wire bytes (null name = empty)",
          "[integration][legacy_bnetd][send_atacceptdecline_bridge]") {
    ScopedSink scope;
    int marker = 0;

    // count=5, action=2 (SERVER_ARRANGEDTEAM_DECLINE), decliner_name=nullptr
    REQUIRE(::pvpgn_v3_send_atmemberdecline(&marker, 5u, 2u, nullptr) == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // header(4) + count(4) + action(4) + "\0"(1) = 13 bytes
    REQUIRE(FakeSink::last_bytes.size() == 13u);

    // header: FF 62 0D 00  (SID_ARRANGEDTEAM_MEMBER_DECLINE = 0x62, size = 13)
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x62u);
    REQUIRE(FakeSink::last_bytes[2] == 13u);
    REQUIRE(FakeSink::last_bytes[3] == 0u);

    // count = 5 LE
    REQUIRE(FakeSink::last_bytes[4] == 5u);
    REQUIRE(FakeSink::last_bytes[5] == 0u);
    REQUIRE(FakeSink::last_bytes[6] == 0u);
    REQUIRE(FakeSink::last_bytes[7] == 0u);

    // action = 2 LE
    REQUIRE(FakeSink::last_bytes[8]  == 2u);
    REQUIRE(FakeSink::last_bytes[9]  == 0u);
    REQUIRE(FakeSink::last_bytes[10] == 0u);
    REQUIRE(FakeSink::last_bytes[11] == 0u);

    // decliner_name = "\0"
    REQUIRE(FakeSink::last_bytes[12] == 0u);
}

TEST_CASE("send_atmemberdecline emits correct wire bytes with decliner name",
          "[integration][legacy_bnetd][send_atacceptdecline_bridge]") {
    ScopedSink scope;
    int marker = 0;

    // count=3, action=2, decliner_name="Dave"
    REQUIRE(::pvpgn_v3_send_atmemberdecline(&marker, 3u, 2u, "Dave") == 1);
    REQUIRE(FakeSink::call_count == 1);

    // header(4) + count(4) + action(4) + "Dave\0"(5) = 17 bytes
    REQUIRE(FakeSink::last_bytes.size() == 17u);

    // header: FF 62 11 00  (0x11 = 17)
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x62u);
    REQUIRE(FakeSink::last_bytes[2] == 17u);
    REQUIRE(FakeSink::last_bytes[3] == 0u);

    // count = 3 LE
    REQUIRE(FakeSink::last_bytes[4] == 3u);
    REQUIRE(FakeSink::last_bytes[5] == 0u);
    REQUIRE(FakeSink::last_bytes[6] == 0u);
    REQUIRE(FakeSink::last_bytes[7] == 0u);

    // action = 2 LE
    REQUIRE(FakeSink::last_bytes[8]  == 2u);
    REQUIRE(FakeSink::last_bytes[9]  == 0u);
    REQUIRE(FakeSink::last_bytes[10] == 0u);
    REQUIRE(FakeSink::last_bytes[11] == 0u);

    // "Dave\0"
    REQUIRE(FakeSink::last_bytes[12] == 'D');
    REQUIRE(FakeSink::last_bytes[13] == 'a');
    REQUIRE(FakeSink::last_bytes[14] == 'v');
    REQUIRE(FakeSink::last_bytes[15] == 'e');
    REQUIRE(FakeSink::last_bytes[16] == 0u);
}

// ---------------------------------------------------------------------------
// Handler return propagation
// ---------------------------------------------------------------------------

TEST_CASE("send_atmemberdecline propagates handler return value",
          "[integration][legacy_bnetd][send_atacceptdecline_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_atmemberdecline(&marker, 0u, 0u, nullptr) == -1);

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_atmemberdecline(&marker, 0u, 0u, nullptr) == 0);
}
