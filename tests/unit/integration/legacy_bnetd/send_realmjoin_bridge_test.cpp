// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_realmjoinreply`.
//
// Verifies:
//   - null conn_ptr returns 0 without calling the handler.
//   - returns 0 when no send_packet handler is installed.
//   - emits correct wire bytes (header + all fixed fields + secret_hash + account_name).
//   - port is written big-endian on the wire.
//   - propagates handler return value (1 / 0 / -1).

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_realmjoin_bridge.hpp"
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

// Zero-filled secret hash for convenience.
static const unsigned int kZeroHash[5] = {0u, 0u, 0u, 0u, 0u};

}  // namespace

// ---------------------------------------------------------------------------
// Null / no-handler guard tests
// ---------------------------------------------------------------------------

TEST_CASE("send_realmjoinreply returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_realmjoin_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);

    int marker = 0;
    REQUIRE(::pvpgn_v3_send_realmjoinreply(
                &marker,
                0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
                kZeroHash, nullptr) == 0);

    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_realmjoinreply rejects null conn pointer",
          "[integration][legacy_bnetd][send_realmjoin_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_realmjoinreply(
                nullptr,
                0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
                kZeroHash, nullptr) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

// ---------------------------------------------------------------------------
// Wire byte correctness
// ---------------------------------------------------------------------------

TEST_CASE("send_realmjoinreply emits correct wire bytes",
          "[integration][legacy_bnetd][send_realmjoin_bridge]") {
    ScopedSink scope;
    int marker = 0;

    // All-zero fields except port=0x1F90 (8080) and account_name="name"
    // port is written big-endian: 0x1F 0x90
    static const unsigned int hash[5] = {0u, 0u, 0u, 0u, 0u};

    REQUIRE(::pvpgn_v3_send_realmjoinreply(
                &marker,
                /*seqno*/0u, /*u1*/0u, /*bncs_addr1*/0u, /*session_num*/0u,
                /*addr*/0u, /*port*/0x1F90u, /*u3*/0u,
                /*session_key*/0u, /*u5*/0u, /*u6*/0u,
                /*client_tag*/0u, /*version_id*/0u, /*bncs_addr2*/0u, /*u7*/0u,
                hash, "name") == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // header(4) +
    // seqno(4)+u1(4)+bncs_addr1(4)+session_num(4)+addr(4) = 20 +
    // port(2)+u3(2) = 4 +
    // session_key(4)+u5(4)+u6(4)+client_tag(4)+version_id(4)+bncs_addr2(4)+u7(4) = 28 +
    // secret_hash[5](20) +
    // "name\0"(5)
    // = 4 + 20 + 4 + 28 + 20 + 5 = 81 bytes
    REQUIRE(FakeSink::last_bytes.size() == 81u);

    // header: FF 3C 51 00  (SID_REALMJOIN = 0x3C, size = 81 = 0x51)
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x3Cu);
    REQUIRE(FakeSink::last_bytes[2] == 81u);
    REQUIRE(FakeSink::last_bytes[3] == 0u);

    // port at offset 4+20 = 24, written big-endian: 0x1F 0x90
    REQUIRE(FakeSink::last_bytes[24] == 0x1Fu);
    REQUIRE(FakeSink::last_bytes[25] == 0x90u);

    // "name\0" at offset 4+20+4+28+20 = 76
    REQUIRE(FakeSink::last_bytes[76] == 'n');
    REQUIRE(FakeSink::last_bytes[77] == 'a');
    REQUIRE(FakeSink::last_bytes[78] == 'm');
    REQUIRE(FakeSink::last_bytes[79] == 'e');
    REQUIRE(FakeSink::last_bytes[80] == 0u);
}

TEST_CASE("send_realmjoinreply encodes secret_hash correctly",
          "[integration][legacy_bnetd][send_realmjoin_bridge]") {
    ScopedSink scope;
    int marker = 0;

    static const unsigned int hash[5] = {
        0x01020304u, 0x05060708u, 0x090A0B0Cu, 0x0D0E0F10u, 0x11121314u
    };

    REQUIRE(::pvpgn_v3_send_realmjoinreply(
                &marker,
                0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
                hash, "") == 1);

    // secret_hash starts at offset 4+20+4+28 = 56
    // hash[0] = 0x01020304 LE: 04 03 02 01
    REQUIRE(FakeSink::last_bytes[56] == 0x04u);
    REQUIRE(FakeSink::last_bytes[57] == 0x03u);
    REQUIRE(FakeSink::last_bytes[58] == 0x02u);
    REQUIRE(FakeSink::last_bytes[59] == 0x01u);

    // hash[4] = 0x11121314 LE: 14 13 12 11
    REQUIRE(FakeSink::last_bytes[72] == 0x14u);
    REQUIRE(FakeSink::last_bytes[73] == 0x13u);
    REQUIRE(FakeSink::last_bytes[74] == 0x12u);
    REQUIRE(FakeSink::last_bytes[75] == 0x11u);
}

// ---------------------------------------------------------------------------
// Handler return propagation
// ---------------------------------------------------------------------------

TEST_CASE("send_realmjoinreply propagates handler return value",
          "[integration][legacy_bnetd][send_realmjoin_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_realmjoinreply(
                &marker,
                0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
                kZeroHash, nullptr) == -1);

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_realmjoinreply(
                &marker,
                0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
                kZeroHash, nullptr) == 0);
}
