// SPDX-License-Identifier: GPL-2.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>

#include <catch2/catch_test_macros.hpp>

#include "protocol/bnet/codec.hpp"
#include "protocol/common/packet.hpp"
#include "protocol/common/writer.hpp"

using namespace pvpgn;

namespace {

// Helper: encode a message and then re-decode it through the framing
// path so tests exercise the real wire bytes, not just struct equality.
template <class Msg, class DecodeFn>
auto round_trip(const Msg& m, DecodeFn decode) {
    protocol::Writer w;
    REQUIRE(protocol::bnet::encode(w, m).has_value());
    auto fp = protocol::parse_packet(w.view());
    REQUIRE(fp.has_value());
    return decode(fp.value().packet);
}

}  // namespace

TEST_CASE("bnet codec: SID_NULL round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    auto r = round_trip(Null{}, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<Null>(r.value()));
}

TEST_CASE("bnet codec: SID_PING round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    Ping in{0xDEADBEEFu};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<Ping>(r.value()));
    REQUIRE(std::get<Ping>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_PING server-direction round-trip",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    Ping in{42};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<Ping>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_AUTH_INFO round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    AuthInfo in;
    in.protocol_id = 0;
    in.platform_id = 0x49583836u;  // 'IX86'
    in.game_id     = 0x53455850u;  // 'SEXP'
    in.version_id  = 0x000000D1u;
    in.language_id = 0x656e5553u;
    in.local_ip    = 0x0100007fu;  // 127.0.0.1
    in.tz_bias     = 0;
    in.mpq_locale  = 0x00000409u;
    in.lang_id     = 0x00000409u;
    in.country_abbr = "USA";
    in.country      = "United States";

    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<AuthInfo>(r.value()));
    REQUIRE(std::get<AuthInfo>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_NULL wire bytes are FF 00 04 00",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    protocol::Writer w;
    REQUIRE(encode(w, Null{}).has_value());
    auto bytes = w.view();
    REQUIRE(bytes.size() == 4);
    REQUIRE(static_cast<std::uint8_t>(bytes[0]) == 0xFF);
    REQUIRE(static_cast<std::uint8_t>(bytes[1]) == 0x00);
    REQUIRE(static_cast<std::uint8_t>(bytes[2]) == 0x04);
    REQUIRE(static_cast<std::uint8_t>(bytes[3]) == 0x00);
}

TEST_CASE("bnet codec: SID_PING wire bytes are FF 25 08 00 <ticks LE>",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    protocol::Writer w;
    REQUIRE(encode(w, Ping{0x11223344u}).has_value());
    auto bytes = w.view();
    REQUIRE(bytes.size() == 8);
    REQUIRE(static_cast<std::uint8_t>(bytes[0]) == 0xFF);
    REQUIRE(static_cast<std::uint8_t>(bytes[1]) == 0x25);
    REQUIRE(static_cast<std::uint8_t>(bytes[2]) == 0x08);
    REQUIRE(static_cast<std::uint8_t>(bytes[3]) == 0x00);
    REQUIRE(static_cast<std::uint8_t>(bytes[4]) == 0x44);
    REQUIRE(static_cast<std::uint8_t>(bytes[5]) == 0x33);
    REQUIRE(static_cast<std::uint8_t>(bytes[6]) == 0x22);
    REQUIRE(static_cast<std::uint8_t>(bytes[7]) == 0x11);
}

TEST_CASE("bnet codec: unknown SID returns Unimplemented",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    // Build a header by hand: marker FF, code 0xAB, size 4.
    std::array<std::byte, 4> raw{
        std::byte{0xFF}, std::byte{0xAB}, std::byte{0x04}, std::byte{0x00}};
    auto fp = protocol::parse_packet(core::ByteView{raw});
    REQUIRE(fp.has_value());
    auto r = decode_client(fp.value().packet);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::Unimplemented);
}

TEST_CASE("bnet codec: SID_NULL with extra body bytes rejected",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    std::array<std::byte, 5> raw{
        std::byte{0xFF}, std::byte{0x00}, std::byte{0x05}, std::byte{0x00},
        std::byte{0x42}};
    auto fp = protocol::parse_packet(core::ByteView{raw});
    REQUIRE(fp.has_value());
    auto r = decode_client(fp.value().packet);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("bnet codec: SID_PING with short body rejected",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    // Says size=6 but ticks needs 4 bytes — only 2 present.
    std::array<std::byte, 6> raw{
        std::byte{0xFF}, std::byte{0x25}, std::byte{0x06}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}};
    auto fp = protocol::parse_packet(core::ByteView{raw});
    REQUIRE(fp.has_value());
    auto r = decode_client(fp.value().packet);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::OutOfRange);
}

TEST_CASE("bnet codec: SID_AUTH_INFO with unterminated country string",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    // 4 header + 36 (9*u32) + "USA\0" + truncated "United"
    protocol::Writer w;
    w.begin_bnet_packet(kSidAuthInfo);
    for (int i = 0; i < 9; ++i) w.write_le<std::uint32_t>(0);
    w.write_cstring("USA");
    // No final NUL — write raw bytes only.
    const char tail[] = "United";
    w.write_bytes(core::as_byte_view(tail, 6));
    REQUIRE(w.finalize_bnet_packet().has_value());

    auto fp = protocol::parse_packet(w.view());
    REQUIRE(fp.has_value());
    auto r = decode_client(fp.value().packet);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::OutOfRange);
}

TEST_CASE("bnet codec: SID_LOGONRESPONSE2 client round-trip",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    LogonResponse2 in{0xAABBCCDDu, 0x11223344u,
                      {0x01020304u, 0x05060708u, 0x090a0b0cu,
                       0x0d0e0f10u, 0x11121314u},
                      "alice"};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<LogonResponse2>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_LOGONRESPONSE2 server reply with reason",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    LogonResponse2Reply in{0x06u, "Account closed: spam"};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<LogonResponse2Reply>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_LOGONRESPONSE2 server reply without reason",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    LogonResponse2Reply in{0x00u, ""};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<LogonResponse2Reply>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_JOINCHANNEL round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    JoinChannel in{1u, "Op Allstars"};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<JoinChannel>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_ENTERCHAT client+server round-trip",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    EnterChatRequest req{"alice", "PXES"};
    auto r1 = round_trip(req, decode_client);
    REQUIRE(r1.has_value());
    REQUIRE(std::get<EnterChatRequest>(r1.value()) == req);

    EnterChatReply rep{"alice#2", "PXES,0,0", "alice"};
    auto r2 = round_trip(rep, decode_server);
    REQUIRE(r2.has_value());
    REQUIRE(std::get<EnterChatReply>(r2.value()) == rep);
}

TEST_CASE("bnet codec: SID_CHATCOMMAND round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ChatCommand in{"/whisper bob hi"};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ChatCommand>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_CHATEVENT round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ChatEvent in{
        /*event_id*/   5,
        /*flags*/      0x10u,
        /*ping*/       42u,
        /*ip*/         0x0100007fu,
        /*acct*/       1234u,
        /*reg*/        0u,
        /*username*/   "bob",
        /*text*/       "hi alice!"};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ChatEvent>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_AUTH_CHECK reply round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    AuthCheckReply in{0x100u, "war3.mpq"};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<AuthCheckReply>(r.value()) == in);
}
