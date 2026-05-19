// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_ladderreply`.
//
// `pvpgn_v3_send_ladderreply` builds SERVER_LADDERREPLY
// (SID_GETLADDERDATA, 0x2E) bytes via the v3 codec and dispatches through the
// registered send_packet handler.  Verifies:
//   - byte-parity vs the legacy on-wire layout:
//       header(4) + client_tag(4) + id(4) + type(4) + start(4) + count(4)
//       = 24 bytes minimum (no entries).
//   - null `conn_ptr` rejection.
//   - returns 0 when no send_packet handler is installed.
//   - propagates the handler return (1 / 0 / -1).

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_ladderreply_bridge.hpp"
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

TEST_CASE("send_ladderreply returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_ladderreply_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);

    int marker = 0;
    REQUIRE(::pvpgn_v3_send_ladderreply(&marker, 0u, 0u, 0u, 0u, 0u, nullptr) == 0);

    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_ladderreply rejects null conn pointer",
          "[integration][legacy_bnetd][send_ladderreply_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_ladderreply(nullptr, 0u, 0u, 0u, 0u, 0u, nullptr) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_ladderreply emits correct wire bytes (no entries)",
          "[integration][legacy_bnetd][send_ladderreply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    // client_tag=0x53544152 ('RATS'), id=1, type=2, start=0, count=0
    REQUIRE(::pvpgn_v3_send_ladderreply(&marker,
                                         0x53544152u, 1u, 2u, 0u, 0u,
                                         nullptr) == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // header(4) + client_tag(4) + id(4) + type(4) + start(4) + count(4) = 24 bytes
    REQUIRE(FakeSink::last_bytes.size() == 24u);

    // header: FF 2E 18 00  (0x2E = SID_GETLADDERDATA, 0x18 = 24)
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x2Eu);
    REQUIRE(FakeSink::last_bytes[2] == 24u);
    REQUIRE(FakeSink::last_bytes[3] == 0u);

    // client_tag LE = 0x53544152 → bytes: 52 41 54 53
    REQUIRE(FakeSink::last_bytes[4] == 0x52u);
    REQUIRE(FakeSink::last_bytes[5] == 0x41u);
    REQUIRE(FakeSink::last_bytes[6] == 0x54u);
    REQUIRE(FakeSink::last_bytes[7] == 0x53u);

    // id LE = 0x00000001
    REQUIRE(FakeSink::last_bytes[8]  == 0x01u);
    REQUIRE(FakeSink::last_bytes[9]  == 0x00u);
    REQUIRE(FakeSink::last_bytes[10] == 0x00u);
    REQUIRE(FakeSink::last_bytes[11] == 0x00u);

    // type LE = 0x00000002
    REQUIRE(FakeSink::last_bytes[12] == 0x02u);
    REQUIRE(FakeSink::last_bytes[13] == 0x00u);
    REQUIRE(FakeSink::last_bytes[14] == 0x00u);
    REQUIRE(FakeSink::last_bytes[15] == 0x00u);

    // start LE = 0x00000000
    REQUIRE(FakeSink::last_bytes[16] == 0x00u);
    REQUIRE(FakeSink::last_bytes[17] == 0x00u);
    REQUIRE(FakeSink::last_bytes[18] == 0x00u);
    REQUIRE(FakeSink::last_bytes[19] == 0x00u);

    // count LE = 0x00000000
    REQUIRE(FakeSink::last_bytes[20] == 0x00u);
    REQUIRE(FakeSink::last_bytes[21] == 0x00u);
    REQUIRE(FakeSink::last_bytes[22] == 0x00u);
    REQUIRE(FakeSink::last_bytes[23] == 0x00u);
}

TEST_CASE("send_ladderreply propagates handler return value",
          "[integration][legacy_bnetd][send_ladderreply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_ladderreply(&marker, 0u, 0u, 0u, 0u, 0u, nullptr) == -1);

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_ladderreply(&marker, 0u, 0u, 0u, 0u, 0u, nullptr) == 0);

    FakeSink::return_value = 1;
    REQUIRE(::pvpgn_v3_send_ladderreply(&marker, 0u, 0u, 0u, 0u, 0u, nullptr) == 1);
}
