// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_authreq1_server`. This bridge covers the
// 1 `packet_create` site in `_client_progident` (handle_bnet.cpp) that
// sends the SERVER_AUTHREQ1 (0x06) CheckRevision challenge.
//
// Wire-format verification:
//   SERVER_AUTHREQ1 (0x06):
//     ff 06 size_le(2) + u64 timestamp(8) + filename\0 + equation\0
//
// Also verifies:
//   - null conn_ptr → returns 0 (no crash)
//   - null filename  → returns 0 (no crash)
//   - null equation  → returns 0 (no crash)
//   - returns 0 when no send_packet handler is installed
//   - propagates handler return values (1 / 0 / -1)

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_authreq1_server_bridge.hpp"
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
// Guard / null-safety tests
// ---------------------------------------------------------------------------

TEST_CASE("send_authreq1_server returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_authreq1_server_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_authreq1_server(
                &marker, 0, "IX86ver1.mpq", "A=A^S") == 0);
    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_authreq1_server rejects null conn pointer",
          "[integration][legacy_bnetd][send_authreq1_server_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_authreq1_server(
                nullptr, 0, "IX86ver1.mpq", "A=A^S") == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_authreq1_server rejects null filename",
          "[integration][legacy_bnetd][send_authreq1_server_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_authreq1_server(
                &marker, 0, nullptr, "A=A^S") == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_authreq1_server rejects null equation",
          "[integration][legacy_bnetd][send_authreq1_server_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_authreq1_server(
                &marker, 0, "IX86ver1.mpq", nullptr) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

// ---------------------------------------------------------------------------
// Wire-format byte-parity test
// ---------------------------------------------------------------------------

TEST_CASE("send_authreq1_server emits correct SERVER_AUTHREQ1 wire format",
          "[integration][legacy_bnetd][send_authreq1_server_bridge]") {
    ScopedSink scope;
    int marker = 0;

    // Use a known timestamp and short strings so we can compute the
    // expected bytes by hand.
    //   timestamp = 0x0102030405060708 (LE: 08 07 06 05 04 03 02 01)
    //   filename  = "AB\0"  (3 bytes)
    //   equation  = "CD\0"  (3 bytes)
    //
    // Total payload after header:
    //   8 (timestamp) + 3 (filename+NUL) + 3 (equation+NUL) = 14 bytes
    // Total packet:
    //   4 (header) + 14 = 18 bytes
    //   size field = 0x0012 LE → 12 00

    constexpr std::uint64_t ts = 0x0102030405060708ULL;
    REQUIRE(::pvpgn_v3_send_authreq1_server(
                &marker, ts, "AB", "CD") == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    const auto& b = FakeSink::last_bytes;
    REQUIRE(b.size() == 18u);

    // BNet header: FF SID=0x06 size=0x0012 LE
    REQUIRE(b[0] == 0xffu);
    REQUIRE(b[1] == 0x06u);
    REQUIRE(b[2] == 0x12u);
    REQUIRE(b[3] == 0x00u);

    // timestamp = 0x0102030405060708 in little-endian
    REQUIRE(b[4]  == 0x08u);
    REQUIRE(b[5]  == 0x07u);
    REQUIRE(b[6]  == 0x06u);
    REQUIRE(b[7]  == 0x05u);
    REQUIRE(b[8]  == 0x04u);
    REQUIRE(b[9]  == 0x03u);
    REQUIRE(b[10] == 0x02u);
    REQUIRE(b[11] == 0x01u);

    // filename "AB\0"
    REQUIRE(b[12] == 'A');
    REQUIRE(b[13] == 'B');
    REQUIRE(b[14] == 0x00u);

    // equation "CD\0"
    REQUIRE(b[15] == 'C');
    REQUIRE(b[16] == 'D');
    REQUIRE(b[17] == 0x00u);
}

TEST_CASE("send_authreq1_server emits correct size for empty strings",
          "[integration][legacy_bnetd][send_authreq1_server_bridge]") {
    ScopedSink scope;
    int marker = 0;

    // Empty filename and equation: each contributes just a NUL byte.
    //   payload: 8 (timestamp) + 1 (NUL) + 1 (NUL) = 10 bytes
    //   total:   4 (header) + 10 = 14 bytes
    //   size field = 0x000E LE → 0E 00
    REQUIRE(::pvpgn_v3_send_authreq1_server(
                &marker, 0ULL, "", "") == 1);

    const auto& b = FakeSink::last_bytes;
    REQUIRE(b.size() == 14u);

    // BNet header: FF 06 0E 00
    REQUIRE(b[0] == 0xffu);
    REQUIRE(b[1] == 0x06u);
    REQUIRE(b[2] == 0x0eu);
    REQUIRE(b[3] == 0x00u);

    // timestamp = 0 → all zero bytes
    for (std::size_t i = 4; i < 12; ++i) {
        REQUIRE(b[i] == 0x00u);
    }

    // empty filename NUL
    REQUIRE(b[12] == 0x00u);
    // empty equation NUL
    REQUIRE(b[13] == 0x00u);
}

// ---------------------------------------------------------------------------
// Handler return-value propagation
// ---------------------------------------------------------------------------

TEST_CASE("send_authreq1_server propagates handler return value 1",
          "[integration][legacy_bnetd][send_authreq1_server_bridge]") {
    ScopedSink scope;
    FakeSink::return_value = 1;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_authreq1_server(
                &marker, 0, "f", "e") == 1);
}

TEST_CASE("send_authreq1_server propagates handler return value 0",
          "[integration][legacy_bnetd][send_authreq1_server_bridge]") {
    ScopedSink scope;
    FakeSink::return_value = 0;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_authreq1_server(
                &marker, 0, "f", "e") == 0);
}

TEST_CASE("send_authreq1_server propagates handler return value -1",
          "[integration][legacy_bnetd][send_authreq1_server_bridge]") {
    ScopedSink scope;
    FakeSink::return_value = -1;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_authreq1_server(
                &marker, 0, "f", "e") == -1);
}
