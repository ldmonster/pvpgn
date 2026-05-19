// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_statsreply`.
//
// `pvpgn_v3_send_statsreply` builds SERVER_STATSREPLY (SID_READUSERDATA, 0x26)
// bytes via the v3 codec and dispatches through the registered send_packet
// handler.  Verifies:
//   - byte-parity vs the legacy on-wire layout:
//       header(4) + name_count(4) + key_count(4) + request_id(4)
//       + NUL-terminated value strings = variable length.
//   - null `conn_ptr` rejection (returns 0).
//   - returns 0 when no send_packet handler is installed.
//   - propagates the handler return (1 / 0 / -1).
//   - null `values` with value_count==0 produces a fixed-size packet.
//   - non-empty values are appended as NUL-terminated strings.

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_statsreply_bridge.hpp"
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
// pvpgn_v3_send_statsreply tests
// ---------------------------------------------------------------------------

TEST_CASE("send_statsreply returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_statsreply_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);

    int marker = 0;
    REQUIRE(::pvpgn_v3_send_statsreply(
                &marker, 0u, 0u, 0u, nullptr, 0u) == 0);

    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_statsreply rejects null conn pointer",
          "[integration][legacy_bnetd][send_statsreply_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_statsreply(
                nullptr, 0u, 0u, 0u, nullptr, 0u) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_statsreply emits correct wire bytes (no values)",
          "[integration][legacy_bnetd][send_statsreply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    // name_count=0, key_count=0, request_id=0, no values
    REQUIRE(::pvpgn_v3_send_statsreply(
                &marker, 0u, 0u, 0u, nullptr, 0u) == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // header(4) + name_count(4) + key_count(4) + request_id(4) = 16 bytes
    REQUIRE(FakeSink::last_bytes.size() == 16u);

    // header: FF 26 10 00  (0x10 = 16)
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x26u);
    REQUIRE(FakeSink::last_bytes[2] == 16u);
    REQUIRE(FakeSink::last_bytes[3] == 0u);

    // name_count LE = 0x00000000
    REQUIRE(FakeSink::last_bytes[4] == 0x00u);
    REQUIRE(FakeSink::last_bytes[5] == 0x00u);
    REQUIRE(FakeSink::last_bytes[6] == 0x00u);
    REQUIRE(FakeSink::last_bytes[7] == 0x00u);

    // key_count LE = 0x00000000
    REQUIRE(FakeSink::last_bytes[8]  == 0x00u);
    REQUIRE(FakeSink::last_bytes[9]  == 0x00u);
    REQUIRE(FakeSink::last_bytes[10] == 0x00u);
    REQUIRE(FakeSink::last_bytes[11] == 0x00u);

    // request_id LE = 0x00000000
    REQUIRE(FakeSink::last_bytes[12] == 0x00u);
    REQUIRE(FakeSink::last_bytes[13] == 0x00u);
    REQUIRE(FakeSink::last_bytes[14] == 0x00u);
    REQUIRE(FakeSink::last_bytes[15] == 0x00u);
}

TEST_CASE("send_statsreply emits correct wire bytes (with values)",
          "[integration][legacy_bnetd][send_statsreply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    // 1 name, 2 keys, request_id=7, values = {"hello", "world"}
    const char* vals[] = {"hello", "world"};
    REQUIRE(::pvpgn_v3_send_statsreply(
                &marker, 1u, 2u, 7u, vals, 2u) == 1);
    REQUIRE(FakeSink::call_count == 1);

    // header(4) + name_count(4) + key_count(4) + request_id(4)
    // + "hello\0"(6) + "world\0"(6) = 28 bytes
    REQUIRE(FakeSink::last_bytes.size() == 28u);

    // header: FF 26 1C 00  (0x1C = 28)
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x26u);
    REQUIRE(FakeSink::last_bytes[2] == 28u);
    REQUIRE(FakeSink::last_bytes[3] == 0u);

    // name_count LE = 0x00000001
    REQUIRE(FakeSink::last_bytes[4] == 0x01u);
    REQUIRE(FakeSink::last_bytes[5] == 0x00u);
    REQUIRE(FakeSink::last_bytes[6] == 0x00u);
    REQUIRE(FakeSink::last_bytes[7] == 0x00u);

    // key_count LE = 0x00000002
    REQUIRE(FakeSink::last_bytes[8]  == 0x02u);
    REQUIRE(FakeSink::last_bytes[9]  == 0x00u);
    REQUIRE(FakeSink::last_bytes[10] == 0x00u);
    REQUIRE(FakeSink::last_bytes[11] == 0x00u);

    // request_id LE = 0x00000007
    REQUIRE(FakeSink::last_bytes[12] == 0x07u);
    REQUIRE(FakeSink::last_bytes[13] == 0x00u);
    REQUIRE(FakeSink::last_bytes[14] == 0x00u);
    REQUIRE(FakeSink::last_bytes[15] == 0x00u);

    // "hello\0"
    REQUIRE(FakeSink::last_bytes[16] == 'h');
    REQUIRE(FakeSink::last_bytes[17] == 'e');
    REQUIRE(FakeSink::last_bytes[18] == 'l');
    REQUIRE(FakeSink::last_bytes[19] == 'l');
    REQUIRE(FakeSink::last_bytes[20] == 'o');
    REQUIRE(FakeSink::last_bytes[21] == 0x00u);

    // "world\0"
    REQUIRE(FakeSink::last_bytes[22] == 'w');
    REQUIRE(FakeSink::last_bytes[23] == 'o');
    REQUIRE(FakeSink::last_bytes[24] == 'r');
    REQUIRE(FakeSink::last_bytes[25] == 'l');
    REQUIRE(FakeSink::last_bytes[26] == 'd');
    REQUIRE(FakeSink::last_bytes[27] == 0x00u);
}

TEST_CASE("send_statsreply propagates handler return value",
          "[integration][legacy_bnetd][send_statsreply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_statsreply(
                &marker, 0u, 0u, 0u, nullptr, 0u) == -1);

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_statsreply(
                &marker, 0u, 0u, 0u, nullptr, 0u) == 0);
}

TEST_CASE("send_statsreply with non-zero request_id encodes correctly",
          "[integration][legacy_bnetd][send_statsreply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    // request_id = 0x12345678
    REQUIRE(::pvpgn_v3_send_statsreply(
                &marker, 2u, 3u, 0x12345678u, nullptr, 0u) == 1);

    // header(4) + name_count(4) + key_count(4) + request_id(4) = 16 bytes
    REQUIRE(FakeSink::last_bytes.size() == 16u);

    // name_count LE = 0x00000002
    REQUIRE(FakeSink::last_bytes[4] == 0x02u);
    REQUIRE(FakeSink::last_bytes[5] == 0x00u);
    REQUIRE(FakeSink::last_bytes[6] == 0x00u);
    REQUIRE(FakeSink::last_bytes[7] == 0x00u);

    // key_count LE = 0x00000003
    REQUIRE(FakeSink::last_bytes[8]  == 0x03u);
    REQUIRE(FakeSink::last_bytes[9]  == 0x00u);
    REQUIRE(FakeSink::last_bytes[10] == 0x00u);
    REQUIRE(FakeSink::last_bytes[11] == 0x00u);

    // request_id LE = 0x12345678
    REQUIRE(FakeSink::last_bytes[12] == 0x78u);
    REQUIRE(FakeSink::last_bytes[13] == 0x56u);
    REQUIRE(FakeSink::last_bytes[14] == 0x34u);
    REQUIRE(FakeSink::last_bytes[15] == 0x12u);
}
