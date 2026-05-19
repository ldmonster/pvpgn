// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_clandisbandreply`.
//
// `pvpgn_v3_send_clandisbandreply` builds SERVER_CLAN_DISBANDREPLY
// (SID_CLAN_DISBAND, 0x73) bytes via the v3 codec and dispatches through the
// registered send_packet handler.  Verifies:
//   - byte-parity vs the legacy on-wire layout:
//       header(4) + cookie(4) + result(1) = 9 bytes.
//   - null `conn_ptr` rejection.
//   - returns 0 when no send_packet handler is installed.
//   - propagates the handler return (1 / 0 / -1).

#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "integration/legacy_bnetd/send_clandisbandreply_bridge.hpp"
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

TEST_CASE("send_clandisbandreply returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_clandisbandreply_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);

    int marker = 0;
    REQUIRE(::pvpgn_v3_send_clandisbandreply(&marker, 0u, 0u) == 0);

    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_clandisbandreply rejects null conn pointer",
          "[integration][legacy_bnetd][send_clandisbandreply_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_clandisbandreply(nullptr, 0u, 0u) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_clandisbandreply emits correct wire bytes (cookie=1, result=0 success)",
          "[integration][legacy_bnetd][send_clandisbandreply_bridge]") {
    ScopedSink scope;
    int marker = 42;

    REQUIRE(::pvpgn_v3_send_clandisbandreply(&marker, 1u, 0u) == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // Wire layout: FF 73 09 00 | 01 00 00 00 | 00
    //   FF = BNET magic, 0x73 = SID_CLAN_DISBAND, 0x0009 = length
    //   cookie=1 (LE u32), result=0
    REQUIRE(FakeSink::last_bytes.size() == 9u);
    REQUIRE(FakeSink::last_bytes[0] == 0xFF);  // BNET magic
    REQUIRE(FakeSink::last_bytes[1] == 0x73);  // SID_CLAN_DISBAND
    REQUIRE(FakeSink::last_bytes[2] == 0x09);  // length lo
    REQUIRE(FakeSink::last_bytes[3] == 0x00);  // length hi
    // cookie = 1 (little-endian)
    REQUIRE(FakeSink::last_bytes[4] == 0x01);
    REQUIRE(FakeSink::last_bytes[5] == 0x00);
    REQUIRE(FakeSink::last_bytes[6] == 0x00);
    REQUIRE(FakeSink::last_bytes[7] == 0x00);
    // result = 0 (success)
    REQUIRE(FakeSink::last_bytes[8] == 0x00);
}

TEST_CASE("send_clandisbandreply emits correct wire bytes (result=1, not authorized)",
          "[integration][legacy_bnetd][send_clandisbandreply_bridge]") {
    ScopedSink scope;
    int marker = 42;

    REQUIRE(::pvpgn_v3_send_clandisbandreply(&marker, 0u, 1u) == 1);
    REQUIRE(FakeSink::last_bytes.size() == 9u);
    REQUIRE(FakeSink::last_bytes[1] == 0x73);
    // result = 1
    REQUIRE(FakeSink::last_bytes[8] == 0x01);
}

TEST_CASE("send_clandisbandreply propagates handler return value",
          "[integration][legacy_bnetd][send_clandisbandreply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_clandisbandreply(&marker, 0u, 0u) == -1);

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_clandisbandreply(&marker, 0u, 0u) == 0);
}
