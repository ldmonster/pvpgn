// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_laddersearchreply`.
//
// `pvpgn_v3_send_laddersearchreply` builds SERVER_LADDERSEARCHREPLY
// (SID_LADDERSEARCH, 0x2F) bytes via the v3 codec and dispatches through the
// registered send_packet handler.  Verifies:
//   - byte-parity vs the legacy on-wire layout:
//       header(4) + rank(4) = 8 bytes.
//   - null `conn_ptr` rejection.
//   - returns 0 when no send_packet handler is installed.
//   - propagates the handler return (1 / 0 / -1).

#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "integration/legacy_bnetd/send_laddersearchreply_bridge.hpp"
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

TEST_CASE("send_laddersearchreply returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_laddersearchreply_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);

    int marker = 0;
    REQUIRE(::pvpgn_v3_send_laddersearchreply(&marker, 0u) == 0);

    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_laddersearchreply rejects null conn pointer",
          "[integration][legacy_bnetd][send_laddersearchreply_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_laddersearchreply(nullptr, 0u) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_laddersearchreply emits correct wire bytes (rank=0, first place)",
          "[integration][legacy_bnetd][send_laddersearchreply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    REQUIRE(::pvpgn_v3_send_laddersearchreply(&marker, 0u) == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // header(4) + rank(4) = 8 bytes
    REQUIRE(FakeSink::last_bytes.size() == 8u);

    // header: FF 2F 08 00  (0x2F = SID_LADDERSEARCH)
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x2Fu);
    REQUIRE(FakeSink::last_bytes[2] == 8u);
    REQUIRE(FakeSink::last_bytes[3] == 0u);

    // rank LE = 0x00000000
    REQUIRE(FakeSink::last_bytes[4] == 0x00u);
    REQUIRE(FakeSink::last_bytes[5] == 0x00u);
    REQUIRE(FakeSink::last_bytes[6] == 0x00u);
    REQUIRE(FakeSink::last_bytes[7] == 0x00u);
}

TEST_CASE("send_laddersearchreply emits correct wire bytes (rank=0xFFFFFFFF, not found)",
          "[integration][legacy_bnetd][send_laddersearchreply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    REQUIRE(::pvpgn_v3_send_laddersearchreply(&marker, 0xFFFFFFFFu) == 1);
    REQUIRE(FakeSink::last_bytes.size() == 8u);

    // rank LE = 0xFFFFFFFF
    REQUIRE(FakeSink::last_bytes[4] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[5] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[6] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[7] == 0xFFu);
}

TEST_CASE("send_laddersearchreply propagates handler return value",
          "[integration][legacy_bnetd][send_laddersearchreply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_laddersearchreply(&marker, 0u) == -1);

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_laddersearchreply(&marker, 0u) == 0);

    FakeSink::return_value = 1;
    REQUIRE(::pvpgn_v3_send_laddersearchreply(&marker, 0u) == 1);
}
