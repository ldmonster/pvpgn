// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_fileinforeply` and `pvpgn_v3_send_pingreply`.
//
// `pvpgn_v3_send_fileinforeply` builds SERVER_FILEINFOREPLY (SID_GETFILETIME,
// 0x33) bytes via the v3 codec and dispatches through the registered
// send_packet handler.  Verifies:
//   - byte-parity vs the legacy on-wire layout (header + u32 type + u32
//     unknown2 + u64 timestamp LE + filename\0).
//   - null `conn_ptr` rejection.
//   - returns 0 when no send_packet handler is installed.
//   - propagates the handler return (1 / 0 / -1).
//   - null filename treated as empty string.
//
// `pvpgn_v3_send_pingreply` builds SERVER_PINGREPLY (SID_NULL, 0x00) — a
// 4-byte header-only packet.  Verifies:
//   - exact 4-byte wire layout (FF 00 04 00).
//   - null `conn_ptr` rejection.
//   - returns 0 when no send_packet handler is installed.
//   - propagates the handler return (1 / 0 / -1).

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_fileinforeply_bridge.hpp"
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
// pvpgn_v3_send_fileinforeply tests
// ---------------------------------------------------------------------------

TEST_CASE("send_fileinforeply returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_fileinforeply_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);

    int marker = 0;
    REQUIRE(::pvpgn_v3_send_fileinforeply(
                &marker, 0u, 0u, 0ull, "tos.txt") == 0);

    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_fileinforeply rejects null conn pointer",
          "[integration][legacy_bnetd][send_fileinforeply_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_fileinforeply(
                nullptr, 0u, 0u, 0ull, "tos.txt") == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_fileinforeply emits correct wire bytes",
          "[integration][legacy_bnetd][send_fileinforeply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    // type=0x00000001, unknown2=0x00000002,
    // timestamp=0x01c79a4dcafebabe, filename="tos.txt"
    REQUIRE(::pvpgn_v3_send_fileinforeply(
                &marker,
                0x00000001u,
                0x00000002u,
                0x01c79a4dcafebabeull,
                "tos.txt") == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // header(4) + type u32(4) + unknown2 u32(4) + timestamp u64(8)
    // + "tos.txt\0"(8) = 28 bytes
    REQUIRE(FakeSink::last_bytes.size() == 28u);

    // header: FF 33 1C 00
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x33u);
    REQUIRE(FakeSink::last_bytes[2] == 28u);
    REQUIRE(FakeSink::last_bytes[3] == 0u);

    // type LE = 0x00000001
    REQUIRE(FakeSink::last_bytes[4] == 0x01u);
    REQUIRE(FakeSink::last_bytes[5] == 0x00u);
    REQUIRE(FakeSink::last_bytes[6] == 0x00u);
    REQUIRE(FakeSink::last_bytes[7] == 0x00u);

    // unknown2 LE = 0x00000002
    REQUIRE(FakeSink::last_bytes[8]  == 0x02u);
    REQUIRE(FakeSink::last_bytes[9]  == 0x00u);
    REQUIRE(FakeSink::last_bytes[10] == 0x00u);
    REQUIRE(FakeSink::last_bytes[11] == 0x00u);

    // timestamp LE low DWORD = 0xcafebabe
    REQUIRE(FakeSink::last_bytes[12] == 0xBEu);
    REQUIRE(FakeSink::last_bytes[13] == 0xBAu);
    REQUIRE(FakeSink::last_bytes[14] == 0xFEu);
    REQUIRE(FakeSink::last_bytes[15] == 0xCAu);

    // timestamp LE high DWORD = 0x01c79a4d
    REQUIRE(FakeSink::last_bytes[16] == 0x4Du);
    REQUIRE(FakeSink::last_bytes[17] == 0x9Au);
    REQUIRE(FakeSink::last_bytes[18] == 0xC7u);
    REQUIRE(FakeSink::last_bytes[19] == 0x01u);

    // filename "tos.txt\0"
    REQUIRE(FakeSink::last_bytes[20] == 't');
    REQUIRE(FakeSink::last_bytes[21] == 'o');
    REQUIRE(FakeSink::last_bytes[22] == 's');
    REQUIRE(FakeSink::last_bytes[23] == '.');
    REQUIRE(FakeSink::last_bytes[24] == 't');
    REQUIRE(FakeSink::last_bytes[25] == 'x');
    REQUIRE(FakeSink::last_bytes[26] == 't');
    REQUIRE(FakeSink::last_bytes[27] == 0u);  // NUL terminator
}

TEST_CASE("send_fileinforeply treats null filename as empty string",
          "[integration][legacy_bnetd][send_fileinforeply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    REQUIRE(::pvpgn_v3_send_fileinforeply(
                &marker, 0u, 0u, 0ull, nullptr) == 1);
    // header(4) + type(4) + unknown2(4) + timestamp(8) + "\0"(1) = 21 bytes
    REQUIRE(FakeSink::last_bytes.size() == 21u);
    REQUIRE(FakeSink::last_bytes[1] == 0x33u);
    REQUIRE(FakeSink::last_bytes[2] == 21u);
    REQUIRE(FakeSink::last_bytes[3] == 0u);
    REQUIRE(FakeSink::last_bytes[20] == 0u);  // NUL for empty filename
}

TEST_CASE("send_fileinforeply propagates handler return values",
          "[integration][legacy_bnetd][send_fileinforeply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_fileinforeply(
                &marker, 0u, 0u, 0ull, "f") == 0);

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_fileinforeply(
                &marker, 0u, 0u, 0ull, "f") == -1);

    FakeSink::return_value = 1;
    REQUIRE(::pvpgn_v3_send_fileinforeply(
                &marker, 0u, 0u, 0ull, "f") == 1);
}

// ---------------------------------------------------------------------------
// pvpgn_v3_send_pingreply tests
// ---------------------------------------------------------------------------

TEST_CASE("send_pingreply returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_fileinforeply_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);

    int marker = 0;
    REQUIRE(::pvpgn_v3_send_pingreply(&marker) == 0);

    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_pingreply rejects null conn pointer",
          "[integration][legacy_bnetd][send_fileinforeply_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_pingreply(nullptr) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_pingreply emits 4-byte SID_NULL header",
          "[integration][legacy_bnetd][send_fileinforeply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    REQUIRE(::pvpgn_v3_send_pingreply(&marker) == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // SID_NULL: FF 00 04 00
    REQUIRE(FakeSink::last_bytes.size() == 4u);
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x00u);
    REQUIRE(FakeSink::last_bytes[2] == 0x04u);
    REQUIRE(FakeSink::last_bytes[3] == 0x00u);
}

TEST_CASE("send_pingreply propagates handler return values",
          "[integration][legacy_bnetd][send_fileinforeply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_pingreply(&marker) == 0);

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_pingreply(&marker) == -1);

    FakeSink::return_value = 1;
    REQUIRE(::pvpgn_v3_send_pingreply(&marker) == 1);
}
