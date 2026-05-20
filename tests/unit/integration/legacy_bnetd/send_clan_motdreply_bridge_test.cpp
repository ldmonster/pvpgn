// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_clan_motdreply`.
//
// Builds SERVER_CLAN_MOTDREPLY (SID_CLAN_MOTD, 0x7c) bytes via the v3 codec
// and dispatches through the registered send_packet handler.

#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "integration/legacy_bnetd/send_clan_motdreply_bridge.hpp"
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

TEST_CASE("send_clan_motdreply returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_clan_motdreply_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);

    int marker = 0;
    REQUIRE(::pvpgn_v3_send_clan_motdreply(&marker, 0u, 0u, "") == 0);

    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_clan_motdreply rejects null conn pointer",
          "[integration][legacy_bnetd][send_clan_motdreply_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_clan_motdreply(nullptr, 0u, 0u, "hi") == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_clan_motdreply emits correct wire bytes (empty motd)",
          "[integration][legacy_bnetd][send_clan_motdreply_bridge]") {
    ScopedSink scope;
    int marker = 42;

    // cookie=0x11223344, unknown1=0xDEADBEEF, motd=""
    REQUIRE(::pvpgn_v3_send_clan_motdreply(
                &marker, 0x11223344u, 0xDEADBEEFu, "") == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // Wire layout: FF 7C 0D 00 | 44 33 22 11 | EF BE AD DE | 00
    //   FF = BNET magic, 0x7C = SID_CLAN_MOTD, 0x000D = length (13)
    //   cookie (LE u32), unknown1 (LE u32), motd null terminator
    REQUIRE(FakeSink::last_bytes.size() == 13u);
    const std::vector<unsigned char> expected = {
        0xFF, 0x7C, 0x0D, 0x00,
        0x44, 0x33, 0x22, 0x11,
        0xEF, 0xBE, 0xAD, 0xDE,
        0x00};
    REQUIRE(FakeSink::last_bytes == expected);
}

TEST_CASE("send_clan_motdreply emits correct wire bytes (with motd text)",
          "[integration][legacy_bnetd][send_clan_motdreply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    // cookie=1, unknown1=0xBBBBBBBB, motd="hi"
    REQUIRE(::pvpgn_v3_send_clan_motdreply(
                &marker, 1u, 0xBBBBBBBBu, "hi") == 1);
    REQUIRE(FakeSink::call_count == 1);

    // 4 (hdr) + 4 (cookie) + 4 (unknown1) + 3 ("hi\0") = 15
    REQUIRE(FakeSink::last_bytes.size() == 15u);
    const std::vector<unsigned char> expected = {
        0xFF, 0x7C, 0x0F, 0x00,
        0x01, 0x00, 0x00, 0x00,
        0xBB, 0xBB, 0xBB, 0xBB,
        'h', 'i', 0x00};
    REQUIRE(FakeSink::last_bytes == expected);
}

TEST_CASE("send_clan_motdreply treats null motd as empty",
          "[integration][legacy_bnetd][send_clan_motdreply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    REQUIRE(::pvpgn_v3_send_clan_motdreply(&marker, 7u, 9u, nullptr) == 1);
    REQUIRE(FakeSink::last_bytes.size() == 13u);
    // Last byte must be null terminator for empty motd.
    REQUIRE(FakeSink::last_bytes.back() == 0x00);
}

TEST_CASE("send_clan_motdreply propagates handler return value",
          "[integration][legacy_bnetd][send_clan_motdreply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_clan_motdreply(&marker, 0u, 0u, "") == 0);

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_clan_motdreply(&marker, 0u, 0u, "") == -1);
}
