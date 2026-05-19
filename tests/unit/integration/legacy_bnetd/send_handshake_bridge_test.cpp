// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_compreply`, `pvpgn_v3_send_sessionkey1`,
// and `pvpgn_v3_send_sessionkey2`. These bridges cover the 4 `packet_create`
// sites in `_client_compinfo1` and `_client_compinfo2` (handle_bnet.cpp).
//
// Wire-format verification:
//   SERVER_COMPREPLY  (0x05): ff 05 14 00 + 4×u32 LE = 20 bytes
//   SERVER_SESSIONKEY1 (0x28): ff 28 08 00 + u32 LE  =  8 bytes
//   SERVER_SESSIONKEY2 (0x1D): ff 1d 0c 00 + 2×u32 LE = 12 bytes
//
// Also verifies:
//   - null conn_ptr → returns 0 (no crash)
//   - returns 0 when no send_packet handler is installed
//   - propagates handler return values (1 / 0 / -1)

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_handshake_bridge.hpp"
#include "integration/legacy_bnetd/send_packet_bridge.hpp"

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

// ---------------------------------------------------------------------------
// pvpgn_v3_send_compreply
// ---------------------------------------------------------------------------

TEST_CASE("send_compreply returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_handshake_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_compreply(&marker) == 0);
    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_compreply rejects null conn pointer",
          "[integration][legacy_bnetd][send_handshake_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_compreply(nullptr) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_compreply emits 20-byte SERVER_COMPREPLY wire format",
          "[integration][legacy_bnetd][send_handshake_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_compreply(&marker) == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // Total: header(4) + reg_version(4) + reg_auth(4) + client_id(4) + client_token(4) = 20
    REQUIRE(FakeSink::last_bytes.size() == 20u);

    // BNet header: ff SID=0x05 size=0x0014 (LE)
    REQUIRE(FakeSink::last_bytes[0] == 0xffu);
    REQUIRE(FakeSink::last_bytes[1] == 0x05u);
    REQUIRE(FakeSink::last_bytes[2] == 0x14u);
    REQUIRE(FakeSink::last_bytes[3] == 0x00u);

    // reg_version = 0x00000001 LE
    REQUIRE(FakeSink::last_bytes[4]  == 0x01u);
    REQUIRE(FakeSink::last_bytes[5]  == 0x00u);
    REQUIRE(FakeSink::last_bytes[6]  == 0x00u);
    REQUIRE(FakeSink::last_bytes[7]  == 0x00u);

    // reg_auth = 0xaa8843d1 LE
    REQUIRE(FakeSink::last_bytes[8]  == 0xd1u);
    REQUIRE(FakeSink::last_bytes[9]  == 0x43u);
    REQUIRE(FakeSink::last_bytes[10] == 0x88u);
    REQUIRE(FakeSink::last_bytes[11] == 0xaau);

    // client_id = 0x001b9dda LE
    REQUIRE(FakeSink::last_bytes[12] == 0xdau);
    REQUIRE(FakeSink::last_bytes[13] == 0x9du);
    REQUIRE(FakeSink::last_bytes[14] == 0x1bu);
    REQUIRE(FakeSink::last_bytes[15] == 0x00u);

    // client_token = 0xab69f79a LE
    REQUIRE(FakeSink::last_bytes[16] == 0x9au);
    REQUIRE(FakeSink::last_bytes[17] == 0xf7u);
    REQUIRE(FakeSink::last_bytes[18] == 0x69u);
    REQUIRE(FakeSink::last_bytes[19] == 0xabu);
}

TEST_CASE("send_compreply propagates handler return values",
          "[integration][legacy_bnetd][send_handshake_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_compreply(&marker) == 0);

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_compreply(&marker) == -1);

    FakeSink::return_value = 1;
    REQUIRE(::pvpgn_v3_send_compreply(&marker) == 1);
}

// ---------------------------------------------------------------------------
// pvpgn_v3_send_sessionkey1
// ---------------------------------------------------------------------------

TEST_CASE("send_sessionkey1 returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_handshake_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_sessionkey1(&marker, 0xdeadbeef) == 0);
    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_sessionkey1 rejects null conn pointer",
          "[integration][legacy_bnetd][send_handshake_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_sessionkey1(nullptr, 0u) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_sessionkey1 emits 8-byte SERVER_SESSIONKEY1 wire format",
          "[integration][legacy_bnetd][send_handshake_bridge]") {
    ScopedSink scope;
    int marker = 0;
    // sessionkey = 0x12345678
    REQUIRE(::pvpgn_v3_send_sessionkey1(&marker, 0x12345678u) == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // Total: header(4) + sessionkey(4) = 8
    REQUIRE(FakeSink::last_bytes.size() == 8u);

    // BNet header: ff SID=0x28 size=0x0008 (LE)
    REQUIRE(FakeSink::last_bytes[0] == 0xffu);
    REQUIRE(FakeSink::last_bytes[1] == 0x28u);
    REQUIRE(FakeSink::last_bytes[2] == 0x08u);
    REQUIRE(FakeSink::last_bytes[3] == 0x00u);

    // sessionkey = 0x12345678 LE
    REQUIRE(FakeSink::last_bytes[4] == 0x78u);
    REQUIRE(FakeSink::last_bytes[5] == 0x56u);
    REQUIRE(FakeSink::last_bytes[6] == 0x34u);
    REQUIRE(FakeSink::last_bytes[7] == 0x12u);
}

TEST_CASE("send_sessionkey1 propagates handler return values",
          "[integration][legacy_bnetd][send_handshake_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_sessionkey1(&marker, 0u) == 0);

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_sessionkey1(&marker, 0u) == -1);

    FakeSink::return_value = 1;
    REQUIRE(::pvpgn_v3_send_sessionkey1(&marker, 0u) == 1);
}

// ---------------------------------------------------------------------------
// pvpgn_v3_send_sessionkey2
// ---------------------------------------------------------------------------

TEST_CASE("send_sessionkey2 returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_handshake_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_sessionkey2(&marker, 0x01u, 0xdeadbeef) == 0);
    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_sessionkey2 rejects null conn pointer",
          "[integration][legacy_bnetd][send_handshake_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_sessionkey2(nullptr, 0u, 0u) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_sessionkey2 emits 12-byte SERVER_SESSIONKEY2 wire format",
          "[integration][legacy_bnetd][send_handshake_bridge]") {
    ScopedSink scope;
    int marker = 0;
    // sessionnum = 0x00000003, sessionkey = 0xaabbccdd
    REQUIRE(::pvpgn_v3_send_sessionkey2(&marker, 0x00000003u, 0xaabbccddu) == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // Total: header(4) + sessionnum(4) + sessionkey(4) = 12
    REQUIRE(FakeSink::last_bytes.size() == 12u);

    // BNet header: ff SID=0x1d size=0x000c (LE)
    REQUIRE(FakeSink::last_bytes[0] == 0xffu);
    REQUIRE(FakeSink::last_bytes[1] == 0x1du);
    REQUIRE(FakeSink::last_bytes[2] == 0x0cu);
    REQUIRE(FakeSink::last_bytes[3] == 0x00u);

    // sessionnum = 0x00000003 LE
    REQUIRE(FakeSink::last_bytes[4] == 0x03u);
    REQUIRE(FakeSink::last_bytes[5] == 0x00u);
    REQUIRE(FakeSink::last_bytes[6] == 0x00u);
    REQUIRE(FakeSink::last_bytes[7] == 0x00u);

    // sessionkey = 0xaabbccdd LE
    REQUIRE(FakeSink::last_bytes[8]  == 0xddu);
    REQUIRE(FakeSink::last_bytes[9]  == 0xccu);
    REQUIRE(FakeSink::last_bytes[10] == 0xbbu);
    REQUIRE(FakeSink::last_bytes[11] == 0xaau);
}

TEST_CASE("send_sessionkey2 propagates handler return values",
          "[integration][legacy_bnetd][send_handshake_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_sessionkey2(&marker, 0u, 0u) == 0);

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_sessionkey2(&marker, 0u, 0u) == -1);

    FakeSink::return_value = 1;
    REQUIRE(::pvpgn_v3_send_sessionkey2(&marker, 0u, 0u) == 1);
}
