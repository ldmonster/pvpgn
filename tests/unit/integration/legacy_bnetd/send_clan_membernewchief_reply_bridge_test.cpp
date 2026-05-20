// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_clan_membernewchief_reply`.
//
// Wire-format verification:
//   SERVER_CLAN_MEMBERNEWCHIEFREPLY (SID 0x74):
//     ff 74 09 00 + u32 LE count + u8 result = 9 bytes total
//
// Also verifies:
//   - null conn_ptr -> returns 0 (no crash, handler not called)
//   - returns 0 when no send_packet handler is installed
//   - propagates handler return values (1 / 0 / -1)

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_clan_membernewchief_reply_bridge.hpp"
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

TEST_CASE("send_clan_membernewchief_reply returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_clan_membernewchief_reply_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_clan_membernewchief_reply(&marker, 0xdeadbeefu, 0x01u) == 0);
    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_clan_membernewchief_reply rejects null conn pointer",
          "[integration][legacy_bnetd][send_clan_membernewchief_reply_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_clan_membernewchief_reply(nullptr, 0u, 0u) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("send_clan_membernewchief_reply emits 9-byte SERVER_CLAN_MEMBERNEWCHIEFREPLY wire format",
          "[integration][legacy_bnetd][send_clan_membernewchief_reply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    // count = 0xcafebabe, result = 0x01 (FAILED)
    REQUIRE(::pvpgn_v3_send_clan_membernewchief_reply(&marker, 0xcafebabeu, 0x01u) == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // Total: header(4) + count(4) + result(1) = 9 bytes
    REQUIRE(FakeSink::last_bytes.size() == 9u);

    // BNet header: ff SID=0x74 size=0x0009 (LE)
    REQUIRE(FakeSink::last_bytes[0] == 0xffu);
    REQUIRE(FakeSink::last_bytes[1] == 0x74u);
    REQUIRE(FakeSink::last_bytes[2] == 0x09u);
    REQUIRE(FakeSink::last_bytes[3] == 0x00u);

    // count = 0xcafebabe LE
    REQUIRE(FakeSink::last_bytes[4] == 0xbeu);
    REQUIRE(FakeSink::last_bytes[5] == 0xbau);
    REQUIRE(FakeSink::last_bytes[6] == 0xfeu);
    REQUIRE(FakeSink::last_bytes[7] == 0xcau);

    // result byte
    REQUIRE(FakeSink::last_bytes[8] == 0x01u);
}

TEST_CASE("send_clan_membernewchief_reply emits success (0x00) result correctly",
          "[integration][legacy_bnetd][send_clan_membernewchief_reply_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(::pvpgn_v3_send_clan_membernewchief_reply(&marker, 0u, 0u) == 1);
    REQUIRE(FakeSink::last_bytes.size() == 9u);
    REQUIRE(FakeSink::last_bytes[8] == 0x00u);
}

TEST_CASE("send_clan_membernewchief_reply propagates handler return values",
          "[integration][legacy_bnetd][send_clan_membernewchief_reply_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_clan_membernewchief_reply(&marker, 0u, 0u) == 0);

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_clan_membernewchief_reply(&marker, 0u, 0u) == -1);

    FakeSink::return_value = 1;
    REQUIRE(::pvpgn_v3_send_clan_membernewchief_reply(&marker, 0u, 0u) == 1);
}
