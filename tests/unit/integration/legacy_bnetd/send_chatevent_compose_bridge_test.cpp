// SPDX-License-Identifier: GPL-2.0-or-later
// Unit tests for `pvpgn_v3_send_chatevent_compose`.
// Same wire layout as `send_chatevent_bridge` (legacy
// `t_server_message`), but the bridge derives `event_id` / `flags` /
// `latency` / `username` / `text` from the pre-resolved `ComposeRequest`
// passed via the C ABI args, routed through `application_chat::compose_chat_event`.

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_chatevent_compose_bridge.hpp"
#include "integration/legacy_bnetd/send_packet_bridge.hpp"

namespace ila = pvpgn::integration::legacy_bnetd;

namespace {

struct FakeSink {
    static inline int     return_value = 1;
    static inline int     call_count   = 0;
    static inline std::vector<unsigned char> last_bytes{};

    static void reset() noexcept {
        return_value = 1;
        call_count   = 0;
        last_bytes.clear();
    }
    static int handler(void* /*conn_ptr*/, void const* bytes,
                       unsigned int size) noexcept {
        ++call_count;
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

// Helper: full Talk request producing a 35-byte packet identical to
// the established SID_CHATEVENT byte-parity fixture.
int call_talk(void* conn) {
    return ::pvpgn_v3_send_chatevent_compose(
        conn,
        /*legacy_type=*/4u,        // message_type_talk index in the legacy enum
        /*me_present=*/1,
        /*me_flags=*/0u,
        /*me_latency=*/0u,
        /*dstflags=*/0u,
        /*dstflags_mf_x=*/0,
        /*dst_eq_me=*/0,
        /*channel_flags_bncflags=*/0u,
        /*chatcharname=*/"Bob",
        /*chatname=*/nullptr,
        /*playerinfo=*/nullptr,
        /*text=*/"hi",
        /*servername=*/nullptr);
}

}  // namespace

TEST_CASE("compose bridge declines with no handler installed",
          "[integration][legacy_bnetd][send_chatevent_compose_bridge]") {
    auto* saved = ila::get_send_packet_handler();
    ila::set_send_packet_handler(nullptr);
    int marker = 0;
    REQUIRE(call_talk(&marker) == 0);
    ila::set_send_packet_handler(saved);
}

TEST_CASE("compose bridge rejects null conn pointer",
          "[integration][legacy_bnetd][send_chatevent_compose_bridge]") {
    ScopedSink scope;
    REQUIRE(call_talk(nullptr) == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("compose bridge TALK byte parity with legacy fixture",
          "[integration][legacy_bnetd][send_chatevent_compose_bridge]") {
    ScopedSink scope;
    int marker = 0;
    REQUIRE(call_talk(&marker) == 1);
    const unsigned char expected[] = {
        0xFF, 0x0F, 0x23, 0x00,             // header, size = 35
        0x05, 0x00, 0x00, 0x00,             // type = TALK (compose-derived)
        0x00, 0x00, 0x00, 0x00,             // flags
        0x00, 0x00, 0x00, 0x00,             // latency
        0x00, 0x00, 0x00, 0x00,             // player_ip
        0x0D, 0xF0, 0xAD, 0xBA,             // account_num magic
        0x0D, 0xF0, 0xAD, 0xBA,             // reg_auth magic
        0x42, 0x6F, 0x62, 0x00,             // "Bob"
        0x68, 0x69, 0x00                    // "hi"
    };
    REQUIRE(FakeSink::last_bytes.size() == sizeof(expected));
    REQUIRE(std::memcmp(FakeSink::last_bytes.data(),
                        expected, sizeof(expected)) == 0);
}

TEST_CASE("compose bridge TALK rejects MF_X (decline)",
          "[integration][legacy_bnetd][send_chatevent_compose_bridge]") {
    ScopedSink scope;
    int marker = 0;
    int rc = ::pvpgn_v3_send_chatevent_compose(
        &marker, /*legacy_type=*/4u, /*me_present=*/1,
        0u, 0u, 0u, /*dstflags_mf_x=*/1, 0, 0u,
        "Bob", nullptr, nullptr, "hi", nullptr);
    REQUIRE(rc == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("compose bridge Info maps to event_id 0x12 with empty username",
          "[integration][legacy_bnetd][send_chatevent_compose_bridge]") {
    ScopedSink scope;
    int marker = 0;
    int rc = ::pvpgn_v3_send_chatevent_compose(
        &marker,
        /*legacy_type=*/13u,    // message_type_info
        /*me_present=*/1,
        0xFFu, 0xFFu, 0xFFu,    // flags/latency/dstflags ignored for Info
        0, 0, 0u,
        "Bob", nullptr, nullptr, "ok", nullptr);
    REQUIRE(rc == 1);
    // Wire: header(4) + u32 event_id(0x12) + 5*u32(=0) + "\0" + "ok\0"
    //     = 4 + 24 + 1 + 3 = 32 bytes.
    REQUIRE(FakeSink::last_bytes.size() == 32u);
    REQUIRE(FakeSink::last_bytes[1] == 0x0Fu);
    REQUIRE(FakeSink::last_bytes[4] == 0x12u);   // INFO subtype
    REQUIRE(FakeSink::last_bytes[5] == 0x00u);
    // flags / latency / player_ip all zero (Info sets them to 0)
    for (int i = 8; i < 20; ++i) {
        REQUIRE(FakeSink::last_bytes[i] == 0x00u);
    }
    // empty username at offset 28, "ok\0" at 29..31.
    REQUIRE(FakeSink::last_bytes[28] == 0x00u);
    REQUIRE(FakeSink::last_bytes[29] == 0x6Fu);  // 'o'
    REQUIRE(FakeSink::last_bytes[30] == 0x6Bu);  // 'k'
    REQUIRE(FakeSink::last_bytes[31] == 0x00u);
}

TEST_CASE("compose bridge declines unknown legacy_type",
          "[integration][legacy_bnetd][send_chatevent_compose_bridge]") {
    ScopedSink scope;
    int marker = 0;
    int rc = ::pvpgn_v3_send_chatevent_compose(
        &marker,
        /*legacy_type=*/99u,
        1, 0u, 0u, 0u, 0, 0, 0u,
        "Bob", nullptr, nullptr, "hi", nullptr);
    REQUIRE(rc == 0);
    REQUIRE(FakeSink::call_count == 0);
}

TEST_CASE("compose bridge propagates handler transport failure",
          "[integration][legacy_bnetd][send_chatevent_compose_bridge]") {
    ScopedSink scope;
    FakeSink::return_value = -1;
    int marker = 0;
    REQUIRE(call_talk(&marker) == -1);
    REQUIRE(FakeSink::call_count == 1);
}
