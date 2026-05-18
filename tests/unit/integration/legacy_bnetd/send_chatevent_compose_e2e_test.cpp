// SPDX-License-Identifier: GPL-2.0-or-later
// End-to-end roundtrip test for `pvpgn_v3_send_chatevent_compose`.
//
// Drives the compose bridge with a `ComposeRequest`, captures the
// emitted bytes via a fake `SendPacketHandler`, then frames + decodes
// them through the v3 `protocol::bnet` codec and asserts the recovered
// `ChatEvent` matches the legacy semantics of `message_bnet_format`
// for the requested `LegacyMessageType`. This proves the full
// caller -> compose -> encode -> wire -> decode pipeline.

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <variant>
#include <vector>

#include "core/bytes.hpp"
#include "integration/legacy_bnetd/send_chatevent_compose_bridge.hpp"
#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/packet.hpp"

namespace ila = pvpgn::integration::legacy_bnetd;
namespace pb  = pvpgn::protocol::bnet;
namespace pp  = pvpgn::protocol;

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

// Frame + decode the captured bytes via the v3 codec, returning the
// recovered ChatEvent.
pb::ChatEvent decode_captured() {
    REQUIRE_FALSE(FakeSink::last_bytes.empty());
    // Reinterpret the captured byte vector as a `core::ByteView`.
    pvpgn::core::ByteView view{
        reinterpret_cast<std::byte const*>(FakeSink::last_bytes.data()),
        FakeSink::last_bytes.size()};
    auto fp = pp::parse_packet(view);
    REQUIRE(fp.has_value());
    auto decoded = pb::decode_server(fp.value().packet);
    REQUIRE(decoded.has_value());
    REQUIRE(std::holds_alternative<pb::ChatEvent>(decoded.value()));
    return std::get<pb::ChatEvent>(decoded.value());
}

constexpr std::uint32_t kMagic = 0xBAADF00Du;

}  // namespace

TEST_CASE("e2e: TALK roundtrips with combined flags + latency",
          "[integration][legacy_bnetd][e2e]") {
    ScopedSink scope;
    int marker = 0;
    int rc = ::pvpgn_v3_send_chatevent_compose(
        &marker,
        /*legacy_type=*/4u,            // message_type_talk
        /*me_present=*/1,
        /*me_flags=*/0x10u,
        /*me_latency=*/42u,
        /*dstflags=*/0x80u,
        /*dstflags_mf_x=*/0,
        /*dst_eq_me=*/0,
        /*channel_flags_bncflags=*/0u,
        "Bob", nullptr, nullptr, "hi alice!", nullptr);
    REQUIRE(rc == 1);
    auto ev = decode_captured();
    CHECK(ev.event_id     == 0x05u);                     // TALK
    CHECK(ev.flags        == (0x10u | 0x80u));
    CHECK(ev.ping_ms      == 42u);
    CHECK(ev.user_ip      == 0u);
    CHECK(ev.acct_number  == kMagic);
    CHECK(ev.registration == kMagic);
    CHECK(ev.username     == "Bob");
    CHECK(ev.text         == "hi alice!");
}

TEST_CASE("e2e: JOIN roundtrips with playerinfo in text slot",
          "[integration][legacy_bnetd][e2e]") {
    ScopedSink scope;
    int marker = 0;
    int rc = ::pvpgn_v3_send_chatevent_compose(
        &marker,
        /*legacy_type=*/1u,            // message_type_join
        1, 0x20u, 10u, 0x0u, 0, 0, 0u,
        "alice", nullptr, "1RAW", "", nullptr);
    REQUIRE(rc == 1);
    auto ev = decode_captured();
    CHECK(ev.event_id == 0x02u);                          // JOIN
    CHECK(ev.flags    == 0x20u);
    CHECK(ev.ping_ms  == 10u);
    CHECK(ev.username == "alice");
    CHECK(ev.text     == "1RAW");                         // playerinfo
}

TEST_CASE("e2e: CHANNEL roundtrips with bncflags + chatname",
          "[integration][legacy_bnetd][e2e]") {
    ScopedSink scope;
    int marker = 0;
    int rc = ::pvpgn_v3_send_chatevent_compose(
        &marker,
        /*legacy_type=*/6u,            // message_type_channel
        1, 0u, 7u, 0u, 0, 0, /*channel_flags_bncflags=*/0x00000010u,
        nullptr, "Chat", nullptr, "welcome", nullptr);
    REQUIRE(rc == 1);
    auto ev = decode_captured();
    CHECK(ev.event_id == 0x07u);                          // CHANNEL
    CHECK(ev.flags    == 0x00000010u);
    CHECK(ev.ping_ms  == 7u);
    CHECK(ev.username == "Chat");
    CHECK(ev.text     == "welcome");
}

TEST_CASE("e2e: WHISPER without me falls back to servername",
          "[integration][legacy_bnetd][e2e]") {
    ScopedSink scope;
    int marker = 0;
    int rc = ::pvpgn_v3_send_chatevent_compose(
        &marker,
        /*legacy_type=*/3u,            // message_type_whisper
        /*me_present=*/0,
        0u, 0u, /*dstflags=*/0x40u, 0, 0, 0u,
        nullptr, nullptr, nullptr, "system msg",
        /*servername=*/"PvPGN");
    REQUIRE(rc == 1);
    auto ev = decode_captured();
    CHECK(ev.event_id == 0x04u);                          // WHISPER
    CHECK(ev.flags    == 0x40u);
    CHECK(ev.ping_ms  == 0u);
    CHECK(ev.username == "PvPGN");
    CHECK(ev.text     == "system msg");
}

TEST_CASE("e2e: INFO roundtrips with empty username and zeroed flags",
          "[integration][legacy_bnetd][e2e]") {
    ScopedSink scope;
    int marker = 0;
    int rc = ::pvpgn_v3_send_chatevent_compose(
        &marker,
        /*legacy_type=*/13u,           // message_type_info
        1, 0xFFu, 0xFFu, 0xFFu, 0, 0, 0u,
        "Bob", "Bob", nullptr, "server: ok", nullptr);
    REQUIRE(rc == 1);
    auto ev = decode_captured();
    CHECK(ev.event_id == 0x12u);                          // INFO
    CHECK(ev.flags    == 0u);
    CHECK(ev.ping_ms  == 0u);
    CHECK(ev.username == "");
    CHECK(ev.text     == "server: ok");
}

TEST_CASE("e2e: CHANNELFULL roundtrips with all-empty payload",
          "[integration][legacy_bnetd][e2e]") {
    ScopedSink scope;
    int marker = 0;
    int rc = ::pvpgn_v3_send_chatevent_compose(
        &marker,
        /*legacy_type=*/10u,           // message_type_channelfull
        1, 0u, 0u, 0u, 0, 0, 0u,
        nullptr, nullptr, nullptr, "ignored", nullptr);
    REQUIRE(rc == 1);
    auto ev = decode_captured();
    CHECK(ev.event_id == 0x0Du);                          // CHANNELFULL
    CHECK(ev.flags    == 0u);
    CHECK(ev.ping_ms  == 0u);
    CHECK(ev.username == "");
    CHECK(ev.text     == "");                             // legacy drops text
}
