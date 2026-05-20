// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_clan_invitereply`.
//
// Wire-format verification:
//   SERVER_CLAN_INVITEREPLY (SID 0x77): ff 77 09 00 + u32 LE count + u8 result
//   = 9 bytes total
//
// Also verifies:
//   - null conn_ptr -> returns 0 (no crash, handler not called)
//   - returns 0 when no send_packet handler is installed
//   - propagates handler return values (1 / 0 / -1)

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_clan_invitereply_bridge.hpp"
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

TEST_CASE("send_clan_invitereply returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_clan_invitereply_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_clan_invitereply(&marker, 0xdeadbeefu, 0x00u) == 0);
    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_clan_invitereply rejects null conn pointer",
          "[integration][legacy_bnetd][send_clan_invitereply_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_clan_invitereply(nullptr, 0u, 0u) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_clan_invitereply emits 9-byte SERVER_CLAN_INVITEREPLY wire format",
          "[integration][legacy_bnetd][send_clan_invitereply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    // count = 0x12345678, result = 0x06 (CLAN_RESPONSE_ACCEPT in legacy)
    REQUIRE(::pvpgn_v3_send_clan_invitereply(&marker, 0x12345678u, 0x06u) == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // Total: header(4) + count(4) + result(1) = 9 bytes
    REQUIRE(FakeSink::last_bytes.size() == 9u);

    // BNet header: ff SID=0x77 size=0x0009 (LE)
    REQUIRE(FakeSink::last_bytes[0] == 0xffu);
    REQUIRE(FakeSink::last_bytes[1] == 0x77u);
    REQUIRE(FakeSink::last_bytes[2] == 0x09u);
    REQUIRE(FakeSink::last_bytes[3] == 0x00u);

    // count = 0x12345678 LE
    REQUIRE(FakeSink::last_bytes[4] == 0x78u);
    REQUIRE(FakeSink::last_bytes[5] == 0x56u);
    REQUIRE(FakeSink::last_bytes[6] == 0x34u);
    REQUIRE(FakeSink::last_bytes[7] == 0x12u);

    // result byte
    REQUIRE(FakeSink::last_bytes[8] == 0x06u);
}

TEST_CASE("send_clan_invitereply emits zero count and result correctly",
          "[integration][legacy_bnetd][send_clan_invitereply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_clan_invitereply(&marker, 0u, 0u) == 1);
    REQUIRE(FakeSink::last_bytes.size() == 9u);
    REQUIRE(FakeSink::last_bytes[4] == 0x00u);
    REQUIRE(FakeSink::last_bytes[5] == 0x00u);
    REQUIRE(FakeSink::last_bytes[6] == 0x00u);
    REQUIRE(FakeSink::last_bytes[7] == 0x00u);
    REQUIRE(FakeSink::last_bytes[8] == 0x00u);
}

TEST_CASE("send_clan_invitereply propagates handler return values",
          "[integration][legacy_bnetd][send_clan_invitereply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_clan_invitereply(&marker, 0u, 0u) == 0);

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_clan_invitereply(&marker, 0u, 0u) == -1);

    FakeSink::return_value = 1;
    REQUIRE(::pvpgn_v3_send_clan_invitereply(&marker, 0u, 0u) == 1);
}
