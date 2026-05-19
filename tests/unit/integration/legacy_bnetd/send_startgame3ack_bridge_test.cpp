// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_startgame3ack`.
//
// `pvpgn_v3_send_startgame3ack` builds SERVER_STARTGAME3_ACK
// (SID_STARTGAME3, 0x1A) bytes via the v3 codec and dispatches through the
// registered send_packet handler.  Verifies:
//   - byte-parity vs the legacy on-wire layout:
//       header(4) + reply(4) = 8 bytes.
//   - null `conn_ptr` rejection.
//   - returns 0 when no send_packet handler is installed.
//   - propagates the handler return (1 / 0 / -1).

#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "integration/legacy_bnetd/send_startgame3ack_bridge.hpp"

namespace ila = pvpgn::integration::legacy_bnetd;

namespace {

struct FakeSink {
    static inline int     return_value     = 1;
    static inline int     call_count       = 0;
    static inline void*   last_conn        = nullptr;
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

TEST_CASE("send_startgame3ack returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_startgame3ack_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);

    int marker = 0;
    REQUIRE(::pvpgn_v3_send_startgame3ack(&marker, 0u) == 0);

    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_startgame3ack rejects null conn pointer",
          "[integration][legacy_bnetd][send_startgame3ack_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_startgame3ack(nullptr, 0u) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_startgame3ack emits correct wire bytes (reply=0, OK)",
          "[integration][legacy_bnetd][send_startgame3ack_bridge]") {
    ScopedSink scope;
    int marker = 0;

    REQUIRE(::pvpgn_v3_send_startgame3ack(&marker, 0u) == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // header(4) + reply(4) = 8 bytes
    REQUIRE(FakeSink::last_bytes.size() == 8u);

    // header: FF 1A 08 00  (0x1A = SID_STARTGAME3)
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x1Au);
    REQUIRE(FakeSink::last_bytes[2] == 8u);
    REQUIRE(FakeSink::last_bytes[3] == 0u);

    // reply LE = 0x00000000
    REQUIRE(FakeSink::last_bytes[4] == 0x00u);
    REQUIRE(FakeSink::last_bytes[5] == 0x00u);
    REQUIRE(FakeSink::last_bytes[6] == 0x00u);
    REQUIRE(FakeSink::last_bytes[7] == 0x00u);
}

TEST_CASE("send_startgame3ack emits correct wire bytes (reply=1, NO)",
          "[integration][legacy_bnetd][send_startgame3ack_bridge]") {
    ScopedSink scope;
    int marker = 0;

    REQUIRE(::pvpgn_v3_send_startgame3ack(&marker, 1u) == 1);
    REQUIRE(FakeSink::last_bytes.size() == 8u);

    // reply LE = 0x00000001
    REQUIRE(FakeSink::last_bytes[4] == 0x01u);
    REQUIRE(FakeSink::last_bytes[5] == 0x00u);
    REQUIRE(FakeSink::last_bytes[6] == 0x00u);
    REQUIRE(FakeSink::last_bytes[7] == 0x00u);
}

TEST_CASE("send_startgame3ack propagates handler return value",
          "[integration][legacy_bnetd][send_startgame3ack_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_startgame3ack(&marker, 0u) == -1);

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_startgame3ack(&marker, 0u) == 0);
}
