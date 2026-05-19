// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_friendinforeply`.
//
// Verifies:
//   - null conn_ptr returns 0 without calling the handler.
//   - returns 0 when no send_packet handler is installed.
//   - emits correct wire bytes (header + friend_num + type + status +
//     client_tag + game_name cstring).
//   - null game_name treated as empty string.
//   - propagates handler return value (1 / 0 / -1).

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_friendinfo_bridge.hpp"
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

// ---------------------------------------------------------------------------
// Null / no-handler guard tests
// ---------------------------------------------------------------------------

TEST_CASE("send_friendinforeply returns 0 with no send handler installed",
          "[integration][legacy_bnetd][send_friendinfo_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);

    int marker = 0;
    REQUIRE(::pvpgn_v3_send_friendinforeply(&marker, 0u, 0u, 0u, 0u, nullptr) == 0);

    ila::set_send_packet_handler(saved);
}

TEST_CASE("send_friendinforeply rejects null conn pointer",
          "[integration][legacy_bnetd][send_friendinfo_bridge]") {
    ScopedSink scope;
    REQUIRE(::pvpgn_v3_send_friendinforeply(nullptr, 0u, 0u, 0u, 0u, nullptr) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

// ---------------------------------------------------------------------------
// Wire byte correctness
// ---------------------------------------------------------------------------

TEST_CASE("send_friendinforeply emits correct wire bytes (offline friend)",
          "[integration][legacy_bnetd][send_friendinfo_bridge]") {
    ScopedSink scope;
    int marker = 0;

    // friend_num=2, type=0 (NON_MUTUAL), status=0 (OFFLINE), client_tag=0,
    // game_name="" (null -> empty cstring)
    REQUIRE(::pvpgn_v3_send_friendinforeply(&marker, 2u, 0u, 0u, 0u, nullptr) == 1);
    REQUIRE(FakeSink::call_count == 1);
    REQUIRE(FakeSink::last_conn == &marker);

    // header(4) + friend_num(1) + type(1) + status(1) + client_tag(4) +
    // game_name "\0"(1) = 12 bytes
    REQUIRE(FakeSink::last_bytes.size() == 12u);

    // header: FF 66 0C 00  (SID_FRIENDINFO = 0x66, size = 12)
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x66u);
    REQUIRE(FakeSink::last_bytes[2] == 12u);
    REQUIRE(FakeSink::last_bytes[3] == 0u);

    // friend_num = 2
    REQUIRE(FakeSink::last_bytes[4] == 2u);
    // type = 0
    REQUIRE(FakeSink::last_bytes[5] == 0u);
    // status = 0
    REQUIRE(FakeSink::last_bytes[6] == 0u);
    // client_tag LE = 0x00000000
    REQUIRE(FakeSink::last_bytes[7]  == 0u);
    REQUIRE(FakeSink::last_bytes[8]  == 0u);
    REQUIRE(FakeSink::last_bytes[9]  == 0u);
    REQUIRE(FakeSink::last_bytes[10] == 0u);
    // game_name = "\0"
    REQUIRE(FakeSink::last_bytes[11] == 0u);
}

TEST_CASE("send_friendinforeply emits correct wire bytes (in-game friend)",
          "[integration][legacy_bnetd][send_friendinfo_bridge]") {
    ScopedSink scope;
    int marker = 0;

    // friend_num=0, type=1 (MUTUAL), status=3 (PUBLIC_GAME),
    // client_tag=0x57335850 ('W3XP'), game_name="MyGame"
    REQUIRE(::pvpgn_v3_send_friendinforeply(
                &marker, 0u, 1u, 3u, 0x57335850u, "MyGame") == 1);
    REQUIRE(FakeSink::call_count == 1);

    // header(4) + friend_num(1) + type(1) + status(1) + client_tag(4) +
    // "MyGame\0"(7) = 18 bytes
    REQUIRE(FakeSink::last_bytes.size() == 18u);

    // header: FF 66 12 00  (0x12 = 18)
    REQUIRE(FakeSink::last_bytes[0] == 0xFFu);
    REQUIRE(FakeSink::last_bytes[1] == 0x66u);
    REQUIRE(FakeSink::last_bytes[2] == 18u);
    REQUIRE(FakeSink::last_bytes[3] == 0u);

    REQUIRE(FakeSink::last_bytes[4] == 0u);   // friend_num
    REQUIRE(FakeSink::last_bytes[5] == 1u);   // type
    REQUIRE(FakeSink::last_bytes[6] == 3u);   // status

    // client_tag 0x57335850 LE
    REQUIRE(FakeSink::last_bytes[7]  == 0x50u);
    REQUIRE(FakeSink::last_bytes[8]  == 0x58u);
    REQUIRE(FakeSink::last_bytes[9]  == 0x33u);
    REQUIRE(FakeSink::last_bytes[10] == 0x57u);

    // "MyGame\0"
    REQUIRE(FakeSink::last_bytes[11] == 'M');
    REQUIRE(FakeSink::last_bytes[12] == 'y');
    REQUIRE(FakeSink::last_bytes[13] == 'G');
    REQUIRE(FakeSink::last_bytes[14] == 'a');
    REQUIRE(FakeSink::last_bytes[15] == 'm');
    REQUIRE(FakeSink::last_bytes[16] == 'e');
    REQUIRE(FakeSink::last_bytes[17] == 0u);
}

// ---------------------------------------------------------------------------
// Handler return propagation
// ---------------------------------------------------------------------------

TEST_CASE("send_friendinforeply propagates handler return value",
          "[integration][legacy_bnetd][send_friendinfo_bridge]") {
    ScopedSink scope;
    int marker = 0;

    FakeSink::return_value = -1;
    REQUIRE(::pvpgn_v3_send_friendinforeply(&marker, 0u, 0u, 0u, 0u, nullptr) == -1);

    FakeSink::return_value = 0;
    REQUIRE(::pvpgn_v3_send_friendinforeply(&marker, 0u, 0u, 0u, 0u, nullptr) == 0);
}
