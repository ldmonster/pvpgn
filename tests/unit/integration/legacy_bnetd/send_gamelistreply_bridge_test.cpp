// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_gamelistreply`.
//
// `pvpgn_v3_send_gamelistreply` builds SERVER_GAMELISTREPLY
// (SID_GETADVLISTEX, 0x09) bytes via the v3 codec and dispatches through the
// registered send_packet handler.  Verifies:
//   - byte-parity vs the legacy on-wire layout:
//       header(4) + gamecount(4) + sstatus(4) = 12 bytes (no entries).
//   - null `conn_ptr` rejection.
//   - returns 0 when no send_packet handler is installed.
//   - propagates the handler return (1 / 0 / -1).
//   - null entries pointer with count=0 produces empty-list reply.

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_gamelistreply_bridge.hpp"
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

TEST_CASE("send_gamelistreply returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_gamelistreply_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);

    int marker = 0;
    REQUIRE(::pvpgn_v3_send_gamelistreply(&marker, 0u, nullptr, 0u) == 0);

    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_gamelistreply rejects null conn pointer",
          "[integration][legacy_bnetd][send_gamelistreply_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_gamelistreply(nullptr, 0u, nullptr, 0u) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_gamelistreply emits correct wire bytes (no entries, sstatus=0)",
          "[integration][legacy_bnetd][send_gamelistreply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    REQUIRE(::pvpgn_v3_send_gamelistreply(&marker, 0u, nullptr, 0u) == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // header(4) + gamecount(4) + sstatus(4) = 12 bytes
    REQUIRE(FakeSink::last_bytes.size() == 12u);

    // header: FF 09 0C 00  (0x0C = 12)
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x09u);
    REQUIRE(FakeSink::last_bytes[2] == 12u);
    REQUIRE(FakeSink::last_bytes[3] == 0u);

    // gamecount LE = 0x00000000
    REQUIRE(FakeSink::last_bytes[4] == 0x00u);
    REQUIRE(FakeSink::last_bytes[5] == 0x00u);
    REQUIRE(FakeSink::last_bytes[6] == 0x00u);
    REQUIRE(FakeSink::last_bytes[7] == 0x00u);

    // sstatus LE = 0x00000000
    REQUIRE(FakeSink::last_bytes[8]  == 0x00u);
    REQUIRE(FakeSink::last_bytes[9]  == 0x00u);
    REQUIRE(FakeSink::last_bytes[10] == 0x00u);
    REQUIRE(FakeSink::last_bytes[11] == 0x00u);
}

TEST_CASE("send_gamelistreply emits correct sstatus field",
          "[integration][legacy_bnetd][send_gamelistreply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    // sstatus = 0x00000002 (FULL)
    REQUIRE(::pvpgn_v3_send_gamelistreply(&marker, 2u, nullptr, 0u) == 1);
    REQUIRE(FakeSink::last_bytes.size() == 12u);

    // sstatus LE = 0x00000002
    REQUIRE(FakeSink::last_bytes[8]  == 0x02u);
    REQUIRE(FakeSink::last_bytes[9]  == 0x00u);
    REQUIRE(FakeSink::last_bytes[10] == 0x00u);
    REQUIRE(FakeSink::last_bytes[11] == 0x00u);
}

TEST_CASE("send_gamelistreply propagates handler return value",
          "[integration][legacy_bnetd][send_gamelistreply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_gamelistreply(&marker, 0u, nullptr, 0u) == -1);

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_gamelistreply(&marker, 0u, nullptr, 0u) == 0);
}
