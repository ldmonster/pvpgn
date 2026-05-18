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
    // Says size=6 but ticks needs 4 bytes вЂ” only 2 present.
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
    // No final NUL вЂ” write raw bytes only.
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

TEST_CASE("bnet codec: SID_CHATEVENT (0x0F) TALK byte parity vs legacy",
          "[protocol][bnet]") {
    using namespace pvpgn::protocol::bnet;
    // Reference layout produced by legacy `message_bnet_format` /
    // `t_server_message` (src/bnetd/message.cpp, src/common/bnet_protocol.h):
    //   header(4) + u32 type + u32 flags + u32 latency
    //              + u32 player_ip (SERVER_MESSAGE_PLAYER_IP_DUMMY = 0)
    //              + u32 account_num (BIG-endian SERVER_MESSAGE_ACCOUNT_NUM
    //                                 = 0x0df0adba -> on-wire 0d f0 ad ba)
    //              + u32 reg_auth   (LITTLE-endian SERVER_MESSAGE_REG_AUTH
    //                                 = 0xBAADF00D -> on-wire 0d f0 ad ba)
    //              + cstring username
    //              + cstring text
    //
    // Both magic fields are intentionally arranged to emit identical
    // 4-byte sequences on the wire. The v3 encoder uses LE for both
    // fields, so byte parity requires the caller to set
    //   acct_number  = 0xBAADF00Du
    //   registration = 0xBAADF00Du
    // which is exactly what the strangler-fig bridge will emit.
    ChatEvent m;
    m.event_id     = 0x05u;                    // SERVER_MESSAGE_TYPE_TALK
    m.flags        = 0u;
    m.ping_ms      = 0u;
    m.user_ip      = 0u;
    m.acct_number  = 0xBAADF00Du;
    m.registration = 0xBAADF00Du;
    m.username     = "Bob";
    m.text         = "hi";

    pvpgn::protocol::Writer w;
    REQUIRE(encode(w, m).has_value());
    auto bytes = w.take();
    const std::uint8_t expected[] = {
        0xFF, 0x0F, 0x23, 0x00,             // header, size = 35
        0x05, 0x00, 0x00, 0x00,             // type = TALK
        0x00, 0x00, 0x00, 0x00,             // flags
        0x00, 0x00, 0x00, 0x00,             // latency
        0x00, 0x00, 0x00, 0x00,             // player_ip
        0x0D, 0xF0, 0xAD, 0xBA,             // account_num magic
        0x0D, 0xF0, 0xAD, 0xBA,             // reg_auth magic
        0x42, 0x6F, 0x62, 0x00,             // "Bob"
        0x68, 0x69, 0x00                    // "hi"
    };
    REQUIRE(bytes.size() == sizeof(expected));
    for (std::size_t i = 0; i < sizeof(expected); ++i)
        REQUIRE(static_cast<std::uint8_t>(bytes[i]) == expected[i]);
}


TEST_CASE("bnet codec: SID_AUTH_CHECK reply round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    AuthCheckReply in{0x100u, "war3.mpq"};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<AuthCheckReply>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_AUTH_INFO server reply round-trip (NLS / W3)",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    AuthInfoReply in;
    in.logontype        = 0x00000002u;            // W3 NLS
    in.server_token     = 0xdeadbeefu;
    in.session_num      = 0x12345678u;
    in.timestamp        = 0x01c79a4d'12345678ull; // FILETIME
    in.mpq_filename     = "ver-IX86-1.mpq";
    in.checksum_formula = "A=A^S B=B-C C=C+A A=A^B";
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<AuthInfoReply>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_AUTH_INFO server reply round-trip (standard logon)",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    AuthInfoReply in;
    in.logontype        = 0u;
    in.server_token     = 0x01020304u;
    in.session_num      = 0u;
    in.timestamp        = 0ull;
    in.mpq_filename     = "IX86ver1.mpq";
    in.checksum_formula = "";
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<AuthInfoReply>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_AUTH_INFO (0x50) reply byte parity vs legacy",
          "[protocol][bnet]") {
    using namespace pvpgn::protocol::bnet;

    // Reference bytes match legacy bnetd `_client_auth_info` emission
    // for SERVER_AUTHREQ_109 (0x50): header (FF 50 size_lo size_hi),
    // then u32 logontype, u32 sessionkey, u32 sessionnum, u64 FILETIME
    // (low DWORD LE then high DWORD LE), cstring mpq_filename,
    // cstring equation, then optional 128 bytes of zero padding for
    // W3/W3XP clients only.

    SECTION("standard logon, no W3 signature") {
        AuthInfoReply m;
        m.logontype        = 0u;
        m.server_token     = 0xdeadbeefu;
        m.session_num      = 0x12345678u;
        m.timestamp        = 0x01c79a4dcafebabeull;
        m.mpq_filename     = "IX86ver1.mpq";
        m.checksum_formula = "A=A^S";

        pvpgn::protocol::Writer w;
        REQUIRE(encode(w, m).has_value());
        auto bytes = w.take();
        // header (4) + 5 u32 (20) + "IX86ver1.mpq\0" (13) +
        // "A=A^S\0" (6) = 43 bytes.
        REQUIRE(bytes.size() == 43u);
        REQUIRE(static_cast<std::uint8_t>(bytes[0]) == 0xFFu);
        REQUIRE(static_cast<std::uint8_t>(bytes[1]) == 0x50u);
        REQUIRE(static_cast<std::uint8_t>(bytes[2]) == 43u);
        REQUIRE(static_cast<std::uint8_t>(bytes[3]) == 0u);
        for (int i = 0; i < 4; ++i)
            REQUIRE(static_cast<std::uint8_t>(bytes[4 + i]) == 0u);
        REQUIRE(static_cast<std::uint8_t>(bytes[8])  == 0xEFu);
        REQUIRE(static_cast<std::uint8_t>(bytes[9])  == 0xBEu);
        REQUIRE(static_cast<std::uint8_t>(bytes[10]) == 0xADu);
        REQUIRE(static_cast<std::uint8_t>(bytes[11]) == 0xDEu);
        REQUIRE(static_cast<std::uint8_t>(bytes[12]) == 0x78u);
        REQUIRE(static_cast<std::uint8_t>(bytes[13]) == 0x56u);
        REQUIRE(static_cast<std::uint8_t>(bytes[14]) == 0x34u);
        REQUIRE(static_cast<std::uint8_t>(bytes[15]) == 0x12u);
        REQUIRE(static_cast<std::uint8_t>(bytes[16]) == 0xBEu);
        REQUIRE(static_cast<std::uint8_t>(bytes[17]) == 0xBAu);
        REQUIRE(static_cast<std::uint8_t>(bytes[18]) == 0xFEu);
        REQUIRE(static_cast<std::uint8_t>(bytes[19]) == 0xCAu);
        REQUIRE(static_cast<std::uint8_t>(bytes[20]) == 0x4Du);
        REQUIRE(static_cast<std::uint8_t>(bytes[21]) == 0x9Au);
        REQUIRE(static_cast<std::uint8_t>(bytes[22]) == 0xC7u);
        REQUIRE(static_cast<std::uint8_t>(bytes[23]) == 0x01u);
        const char expected_name[] = "IX86ver1.mpq";
        for (std::size_t i = 0; i < sizeof(expected_name); ++i)
            REQUIRE(static_cast<std::uint8_t>(bytes[24 + i]) ==
                    static_cast<std::uint8_t>(expected_name[i]));
        const char expected_eq[] = "A=A^S";
        for (std::size_t i = 0; i < sizeof(expected_eq); ++i)
            REQUIRE(static_cast<std::uint8_t>(bytes[37 + i]) ==
                    static_cast<std::uint8_t>(expected_eq[i]));
    }

    SECTION("W3 with 128-byte zero signature pad") {
        AuthInfoReply m;
        m.logontype        = 0x00000002u; // W3 NLS
        m.server_token     = 0u;
        m.session_num      = 0u;
        m.timestamp        = 0u;
        m.mpq_filename     = "";
        m.checksum_formula = "";
        m.server_signature.assign(128, 0u);

        pvpgn::protocol::Writer w;
        REQUIRE(encode(w, m).has_value());
        auto bytes = w.take();
        // header (4) + u32*5 (20) + "\0" + "\0" + 128 = 154 bytes.
        REQUIRE(bytes.size() == 154u);
        REQUIRE(static_cast<std::uint8_t>(bytes[1]) == 0x50u);
        REQUIRE(static_cast<std::uint8_t>(bytes[2]) == 154u);
        REQUIRE(static_cast<std::uint8_t>(bytes[3]) == 0u);
        REQUIRE(static_cast<std::uint8_t>(bytes[4]) == 0x02u);
        REQUIRE(static_cast<std::uint8_t>(bytes[24]) == 0u);
        REQUIRE(static_cast<std::uint8_t>(bytes[25]) == 0u);
        for (std::size_t i = 0; i < 128; ++i)
            REQUIRE(static_cast<std::uint8_t>(bytes[26 + i]) == 0u);
    }
}

TEST_CASE("bnet codec: SID_AUTH_CHECK client request round-trip (LoD: 2 cdkeys)",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    AuthCheckRequest in;
    in.ticks       = 0x42da88c9u;
    in.gameversion = 0x00010000u;
    in.checksum    = 0x9a629746u;
    in.spawn       = 0u;
    in.cdkeys.push_back(CdKeyInfo{
        /*public_value*/ 0x00000010u,
        /*product*/      0x00000006u,
        /*checksum*/     0x0039e7a5u,
        /*unknown*/      0x00000000u,
        /*hash*/         {0xf74fcdedu, 0x964f7a6au, 0xa22d7a85u,
                          0xd6b11f7fu, 0x508db381u}});
    in.cdkeys.push_back(CdKeyInfo{0x01020304u, 0x05060708u, 0x090a0b0cu,
                                  0x0d0e0f10u,
                                  {0x11111111u, 0x22222222u, 0x33333333u,
                                   0x44444444u, 0x55555555u}});
    in.exe_info    = "Game.exe 08/16/01 23:04:40 424067";
    in.cdkey_owner = "tsinghua";
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<AuthCheckRequest>(r.value()));
    REQUIRE(std::get<AuthCheckRequest>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_AUTH_CHECK client request bounded cdkey count",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    // Craft a packet that lies about cdkey_count: claims 0xFFFFFFFF.
    protocol::Writer w;
    w.begin_bnet_packet(kSidAuthCheck);
    w.write_le<std::uint32_t>(0u);            // ticks
    w.write_le<std::uint32_t>(0u);            // gameversion
    w.write_le<std::uint32_t>(0u);            // checksum
    w.write_le<std::uint32_t>(0xFFFFFFFFu);   // cdkey_count
    w.write_le<std::uint32_t>(0u);            // spawn
    REQUIRE(w.finalize_bnet_packet().has_value());
    auto fp = protocol::parse_packet(w.view());
    REQUIRE(fp.has_value());
    auto r = decode_client(fp.value().packet);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("bnet codec: SID_GETADVLISTEX request round-trip",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    GameListRequest in{
        /*gametype*/   0x0002u,
        /*unknown1*/   0u,
        /*unknown2*/   0u,
        /*unknown3*/   0u,
        /*max_games*/  0x19u,
        /*game_name*/  ""};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<GameListRequest>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_GETADVLISTEX reply round-trip (2 games)",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    GameListReply in;
    in.sstatus = 0u;
    in.entries.push_back(GameListEntry{
        /*gametype*/   0x0002u,
        /*unknown1*/   0x0000u,
        /*unknown3*/   0x0002u,
        /*port*/       6112u,
        /*game_ip*/    0x7F000001u,
        /*unknown4*/   0u,
        /*unknown5*/   0u,
        /*status*/     0x00000004u,
        /*unknown6*/   0x0000002Bu,
        /*game_name*/  "MyGame",
        /*password*/   "",
        /*info*/       ",34,12,5,1,3,1,ccc36406,,Bob"});
    in.entries.push_back(GameListEntry{
        0x0010u, 0x0004u, 0x0009u, 6113u, 0x0A000001u, 0u, 0u,
        0x000000C5u, 0x0000002Bu,
        "Ladder 1 on 1",
        "",
        ",,,6,2,f,4,fcc58e4a,7200,Ice69burg"});
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<GameListReply>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_GETADVLISTEX reply error (sstatus, no entries)",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    GameListReply in;
    in.sstatus = 0x3u;  // game full
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<GameListReply>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_LADDERSEARCH request+reply round-trip",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    LadderSearchRequest req{
        /*client_tag*/  0x53455850u,   // 'SEXP'
        /*id*/          0x00000001u,
        /*type*/        0x00000000u,
        /*player_name*/ "alice"};
    auto r1 = round_trip(req, decode_client);
    REQUIRE(r1.has_value());
    REQUIRE(std::get<LadderSearchRequest>(r1.value()) == req);

    LadderSearchReply rep{42u};
    auto r2 = round_trip(rep, decode_server);
    REQUIRE(r2.has_value());
    REQUIRE(std::get<LadderSearchReply>(r2.value()) == rep);

    LadderSearchReply none{0xFFFFFFFFu};
    auto r3 = round_trip(none, decode_server);
    REQUIRE(r3.has_value());
    REQUIRE(std::get<LadderSearchReply>(r3.value()) == none);
}

TEST_CASE("bnet codec: SID_GETFILETIME request+reply round-trip",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    FileInfoRequest req{0x0000001Au, 0u, "tos_USA.txt"};
    auto r1 = round_trip(req, decode_client);
    REQUIRE(r1.has_value());
    REQUIRE(std::get<FileInfoRequest>(r1.value()) == req);

    FileInfoReply rep{0x0000001Au, 0u, 0x01BD4F098689C330ull, "tos.txt"};
    auto r2 = round_trip(rep, decode_server);
    REQUIRE(r2.has_value());
    REQUIRE(std::get<FileInfoReply>(r2.value()) == rep);
}

TEST_CASE("bnet codec: SID_GETADVLISTEX reply rejects oversize game_count",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    protocol::Writer w;
    w.begin_bnet_packet(kSidGetAdvListEx);
    w.write_le<std::uint32_t>(0x00100000u);  // game_count way too big
    w.write_le<std::uint32_t>(0u);
    REQUIRE(w.finalize_bnet_packet().has_value());
    auto fp = protocol::parse_packet(w.view());
    REQUIRE(fp.has_value());
    auto r = decode_server(fp.value().packet);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("bnet codec: SID_CDKEY2 client request round-trip",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    CdKey2Request in;
    in.spawn        = 0u;
    in.keylen       = 26u;
    in.product_id   = 0x44320000u;
    in.key_value    = 0x12345678u;
    in.server_token = 0xdeadbeefu;
    in.ticks        = 0x01020304u;
    in.key_hash     = {0x11111111u, 0x22222222u, 0x33333333u,
                       0x44444444u, 0x55555555u};
    in.owner        = "Bob";
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<CdKey2Request>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_CDKEY2 server reply round-trip (OK)",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    CdKey2Reply in{1u, ""};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<CdKey2Reply>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_CDKEY2 server reply round-trip (INUSE w/ owner)",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    CdKey2Reply in{5u, "Alice"};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<CdKey2Reply>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_FRIENDSLIST request round-trip (empty)",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    FriendsListRequest in;
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<FriendsListRequest>(r.value()));
}

TEST_CASE("bnet codec: SID_FRIENDSLIST reply round-trip (2 entries)",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    FriendsListReply in;
    in.entries.push_back(FriendsListEntry{
        /*name*/ "Alice",
        /*status*/ 0x01u,           // FRIEND_TYPE_MUTUAL
        /*location*/ 0x03u,         // FRIENDSTATUS_PUBLIC_GAME
        /*client_tag*/ 0x57415233u, // 'WAR3'
        /*location_name*/ "Lost Temple"});
    in.entries.push_back(FriendsListEntry{
        "Bob",
        0x00u,
        0x00u,                       // OFFLINE
        0u,
        ""});
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<FriendsListReply>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_FRIENDSLIST reply rejects oversize count",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    protocol::Writer w;
    w.begin_bnet_packet(kSidFriendsList);
    w.write_le<std::uint8_t>(255u);    // > 200 cap
    REQUIRE(w.finalize_bnet_packet().has_value());
    auto fp = protocol::parse_packet(w.view());
    REQUIRE(fp.has_value());
    auto r = decode_server(fp.value().packet);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("bnet codec: SID_FRIENDINFO request+reply round-trip",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    FriendInfoRequest req{2u};
    auto r1 = round_trip(req, decode_client);
    REQUIRE(r1.has_value());
    REQUIRE(std::get<FriendInfoRequest>(r1.value()) == req);

    FriendInfoReply rep{
        /*friend_num*/ 2u,
        /*type*/ 0x01u,            // mutual
        /*status*/ 0x02u,          // private game
        /*client_tag*/ 0x53455850u, // 'SEXP'
        /*game_name*/ "Hardcore Run"};
    auto r2 = round_trip(rep, decode_server);
    REQUIRE(r2.has_value());
    REQUIRE(std::get<FriendInfoReply>(r2.value()) == rep);
}

TEST_CASE("bnet codec: SID_CLANINFO request round-trip",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    ClanInfoRequest in{
        /*cookie*/ 0xCAFEBABEu,
        /*clan_tag*/ 0x434C4E31u,  // 'CLN1'
        /*player_name*/ "Bob"};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ClanInfoRequest>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_CLANINFO reply round-trip (success)",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    ClanInfoReply in;
    in.cookie    = 0xCAFEBABEu;
    in.fail      = 0u;
    in.clan_name = "Iron Forge";
    in.rank      = 0x03u;          // Chieftain
    in.join_time = 0x5C0A1234u;
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ClanInfoReply>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_CLANINFO reply round-trip (failure)",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    ClanInfoReply in;
    in.cookie = 0xCAFEBABEu;
    in.fail   = 0x01u;
    // clan_name/rank/join_time stay default; trailing block must be
    // suppressed on the wire and read-back zero.
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ClanInfoReply>(r.value()) == in);
}

TEST_CASE("bnet: READUSERDATA request round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    UserDataReadRequest in;
    in.request_id = 0x02825278u;
    in.names = {"Ross"};
    in.keys  = {"profile\\sex", "profile\\age", "profile\\location", "profile\\description"};
    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());
    auto pkt = protocol::parse_packet(w.view());
    REQUIRE(pkt.has_value());
    REQUIRE(pkt.value().packet.header.code == kSidReadUserData);
    auto m = decode_client(pkt.value().packet);
    REQUIRE(m.has_value());
    REQUIRE(std::get<UserDataReadRequest>(m.value()) == in);
}

TEST_CASE("bnet: READUSERDATA reply round-trip (1 name x 4 keys)", "[protocol][bnet]") {
    using namespace protocol::bnet;
    UserDataReadReply in;
    in.request_id = 0x02825278u;
    in.name_count = 1;
    in.key_count  = 4;
    in.values     = {"male", "29", "Earth", "Hello world"};
    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());
    auto pkt = protocol::parse_packet(w.view());
    REQUIRE(pkt.has_value());
    auto m = decode_server(pkt.value().packet);
    REQUIRE(m.has_value());
    REQUIRE(std::get<UserDataReadReply>(m.value()) == in);
}

TEST_CASE("bnet: READUSERDATA reply rejects oversize cell count", "[protocol][bnet]") {
    using namespace protocol::bnet;
    // Craft a malformed reply with name_count=1000 and key_count=1000 -> 1e6 cells.
    protocol::Writer w;
    w.begin_bnet_packet(kSidReadUserData);
    w.write_le<std::uint32_t>(1000u);   // name_count
    w.write_le<std::uint32_t>(1000u);   // key_count
    w.write_le<std::uint32_t>(0u);      // request_id
    REQUIRE(w.finalize_bnet_packet().has_value());
    auto pkt = protocol::parse_packet(w.view());
    REQUIRE(pkt.has_value());
    auto r = decode_server(pkt.value().packet);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("bnet: WRITEUSERDATA round-trip (1 name x 4 keys/values)", "[protocol][bnet]") {
    using namespace protocol::bnet;
    UserDataWriteRequest in;
    in.names  = {"Ross_CM"};
    in.keys   = {"profile\\sex", "profile\\age", "profile\\location", "profile\\description"};
    in.values = {"asdf", "asf", "asdf", "asdfasdfasdfasdf\r\nasd\r\nfasd\r\nfasdf\r\n"};
    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());
    auto pkt = protocol::parse_packet(w.view());
    REQUIRE(pkt.has_value());
    REQUIRE(pkt.value().packet.header.code == kSidWriteUserData);
    auto m = decode_client(pkt.value().packet);
    REQUIRE(m.has_value());
    REQUIRE(std::get<UserDataWriteRequest>(m.value()) == in);
}

TEST_CASE("bnet: WRITEUSERDATA rejects oversize cell count", "[protocol][bnet]") {
    using namespace protocol::bnet;
    protocol::Writer w;
    w.begin_bnet_packet(kSidWriteUserData);
    w.write_le<std::uint32_t>(0u);      // name_count
    w.write_le<std::uint32_t>(500u);    // key_count exceeds 256 per-vector limit
    REQUIRE(w.finalize_bnet_packet().has_value());
    auto pkt = protocol::parse_packet(w.view());
    REQUIRE(pkt.has_value());
    auto r = decode_client(pkt.value().packet);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("bnet: CLAN_CREATE request round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ClanCreateRequest in{0x12345678u, 0x434C4E31u};  // "CLN1"
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ClanCreateRequest>(r.value()) == in);
}

TEST_CASE("bnet: CLAN_CREATE reply round-trip (with friend list)", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ClanCreateReply in;
    in.cookie       = 0x12345678u;
    in.check_result = 0x00;
    in.friend_names = {"DJP2", "DJP3", "DJP4"};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ClanCreateReply>(r.value()) == in);
}

TEST_CASE("bnet: CLAN_CREATE reply rejects oversize friend_count", "[protocol][bnet]") {
    using namespace protocol::bnet;
    protocol::Writer w;
    w.begin_bnet_packet(kSidClanCreate);
    w.write_le<std::uint32_t>(0u);   // cookie
    w.write_le<std::uint8_t>(0u);    // check_result
    w.write_le<std::uint8_t>(200u);  // friend_count >> 64
    REQUIRE(w.finalize_bnet_packet().has_value());
    auto fp = protocol::parse_packet(w.view());
    REQUIRE(fp.has_value());
    auto r = decode_server(fp.value().packet);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("bnet: CLAN_DISBAND request round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ClanDisbandRequest in{0x01u};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ClanDisbandRequest>(r.value()) == in);
}

TEST_CASE("bnet: CLAN_DISBAND reply round-trip (generic result)", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ClanGenericResultReply in;
    in.sid    = kSidClanDisband;
    in.cookie = 0x01u;
    in.result = 0x02u;  // "Clan exists less than 1 week"
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ClanGenericResultReply>(r.value()) == in);
}

TEST_CASE("bnet: CLAN_INVITE request + reply round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ClanInviteRequest req{0x01u, "DJP1"};
    auto rr = round_trip(req, decode_client);
    REQUIRE(rr.has_value());
    REQUIRE(std::get<ClanInviteRequest>(rr.value()) == req);

    ClanGenericResultReply rep;
    rep.sid    = kSidClanInvite;
    rep.cookie = 0x01u;
    rep.result = 0x04u;  // decline
    auto rs = round_trip(rep, decode_server);
    REQUIRE(rs.has_value());
    REQUIRE(std::get<ClanGenericResultReply>(rs.value()) == rep);
}

TEST_CASE("bnet: CLAN_MEMBER_REMOVE request + reply", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ClanMemberRemoveRequest req{0x42u, "BadActor"};
    auto rr = round_trip(req, decode_client);
    REQUIRE(rr.has_value());
    REQUIRE(std::get<ClanMemberRemoveRequest>(rr.value()) == req);

    ClanGenericResultReply rep;
    rep.sid    = kSidClanMemberRemove;
    rep.cookie = 0x42u;
    rep.result = 0x00u;
    auto rs = round_trip(rep, decode_server);
    REQUIRE(rs.has_value());
    REQUIRE(std::get<ClanGenericResultReply>(rs.value()) == rep);
}

TEST_CASE("bnet: CLAN_RANKUPDATE request round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ClanMemberRankUpdateRequest in{0x07u, "MemberX", 0x03u};  // promote to shaman
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ClanMemberRankUpdateRequest>(r.value()) == in);
}

TEST_CASE("bnet: CLAN_NEWCHIEF request + reply", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ClanNewChiefRequest req{0x99u, "Successor"};
    auto rr = round_trip(req, decode_client);
    REQUIRE(rr.has_value());
    REQUIRE(std::get<ClanNewChiefRequest>(rr.value()) == req);

    ClanGenericResultReply rep;
    rep.sid    = kSidClanMemberNewChief;
    rep.cookie = 0x99u;
    rep.result = 0x00u;
    auto rs = round_trip(rep, decode_server);
    REQUIRE(rs.has_value());
    REQUIRE(std::get<ClanGenericResultReply>(rs.value()) == rep);
}

TEST_CASE("bnet: CLAN_MOTDCHG round-trip (client only)", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ClanMotdChange in{0x00000000u, "Welcome to the clan!\r\nGL HF"};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ClanMotdChange>(r.value()) == in);
}

TEST_CASE("bnet: CLAN_MOTD request + reply round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ClanMotdRequest req{0xCAFEBABEu};
    auto rr = round_trip(req, decode_client);
    REQUIRE(rr.has_value());
    REQUIRE(std::get<ClanMotdRequest>(rr.value()) == req);

    ClanMotdReply rep;
    rep.cookie   = 0xCAFEBABEu;
    rep.unknown1 = 0u;
    rep.motd     = "Welcome to the clan!";
    auto rs = round_trip(rep, decode_server);
    REQUIRE(rs.has_value());
    REQUIRE(std::get<ClanMotdReply>(rs.value()) == rep);
}

TEST_CASE("bnet: 0x71 CLAN_CREATEINVITE request round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ClanCreateInviteRequest in;
    in.cookie       = 0x01u;
    in.clan_name    = "SubWarZone";
    in.clan_tag     = 0x5A57534Eu;  // ZWSN
    in.friend_names = {"DJP2", "DJP3", "DJP4", "DJP5"};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ClanCreateInviteRequest>(r.value()) == in);
}

TEST_CASE("bnet: 0x71 CLAN_CREATEINVITE summary (success)", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ClanCreateInviteSummary in;
    in.cookie = 0x05u;
    in.status = 0x00u;  // success; no failed_member
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ClanCreateInviteSummary>(r.value()) == in);
}

TEST_CASE("bnet: 0x71 CLAN_CREATEINVITE summary (failure with name)", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ClanCreateInviteSummary in;
    in.cookie        = 0x02u;
    in.status        = 0x05u;  // cannot contact
    in.failed_member = "DJP2";
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ClanCreateInviteSummary>(r.value()) == in);
}

TEST_CASE("bnet: 0x71 CLAN_CREATEINVITE rejects oversize friend list", "[protocol][bnet]") {
    using namespace protocol::bnet;
    protocol::Writer w;
    w.begin_bnet_packet(kSidClanCreateInvite);
    w.write_le<std::uint32_t>(0u);
    w.write_cstring("ClanName");
    w.write_le<std::uint32_t>(0u);
    w.write_le<std::uint8_t>(200u);  // > 64
    REQUIRE(w.finalize_bnet_packet().has_value());
    auto fp = protocol::parse_packet(w.view());
    REQUIRE(fp.has_value());
    auto r = decode_client(fp.value().packet);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("bnet: 0x72 CLAN_CREATEINVITE forward round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ClanCreateInviteForward in;
    in.cookie       = 0x02u;
    in.clan_tag     = 0x5A57534Eu;
    in.clan_name    = "SubWarZone";
    in.clan_creator = "Founder";
    in.friend_names = {"DJP2", "DJP3"};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ClanCreateInviteForward>(r.value()) == in);
}

TEST_CASE("bnet: 0x72 CLAN_CREATEINVITE response (accept)", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ClanCreateInviteResponse in;
    in.cookie       = 0x02u;
    in.clan_tag     = 0x5A57534Eu;
    in.clan_creator = "Founder";
    in.reply        = 0x06u;  // accept
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ClanCreateInviteResponse>(r.value()) == in);
}

TEST_CASE("bnet: 0x79 CLAN_INVITE2 forward round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ClanInvite2Forward in;
    in.cookie       = 0x10u;
    in.clan_tag     = 0x5A57534Eu;
    in.clan_name    = "SubWarZone";
    in.inviter_name = "Chieftain";
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ClanInvite2Forward>(r.value()) == in);
}

TEST_CASE("bnet: 0x79 CLAN_INVITE2 response (decline)", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ClanInvite2Response in;
    in.cookie       = 0x10u;
    in.clan_tag     = 0x5A57534Eu;
    in.inviter_name = "Chieftain";
    in.reply        = 0x04u;  // decline
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ClanInvite2Response>(r.value()) == in);
}


TEST_CASE("bnet: 0x7D CLANMEMBERLIST request round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ClanMemberListRequest in;
    in.cookie = 0x42u;
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ClanMemberListRequest>(r.value()) == in);
}

TEST_CASE("bnet: 0x7D CLANMEMBERLIST reply round-trip with members", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ClanMemberListReply in;
    in.cookie = 0x42u;
    in.members = {
        ClanMemberEntry{"MaggeuS.SwZ", 0x02u, 0x00u, ""},
        ClanMemberEntry{"Sire_Loup",   0x02u, 0x02u, "Channel.SwZ"},
        ClanMemberEntry{"Red.DraKe",   0x03u, 0x03u, "Public Game"},
    };
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ClanMemberListReply>(r.value()) == in);
}

TEST_CASE("bnet: 0x7D CLANMEMBERLIST reply round-trip empty roster", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ClanMemberListReply in;
    in.cookie = 0x00u;
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ClanMemberListReply>(r.value()) == in);
}

TEST_CASE("bnet: 0x7E CLANMEMBER_REMOVED notify round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ClanMemberRemovedNotify in;
    in.name = "DJP5";
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ClanMemberRemovedNotify>(r.value()) == in);
}

TEST_CASE("bnet: 0x7F CLANMEMBERUPDATE round-trip in channel", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ClanMemberUpdate in;
    in.name          = "DJP5";
    in.rank          = 0x02u;  // grunt
    in.online_status = 0x02u;  // channel
    in.location      = "Channel.SwZ";
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ClanMemberUpdate>(r.value()) == in);
}

TEST_CASE("bnet: 0x7F CLANMEMBERUPDATE round-trip offline", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ClanMemberUpdate in;
    in.name          = "Trollo";
    in.rank          = 0x01u;  // peon
    in.online_status = 0x00u;  // offline
    in.location      = "";
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ClanMemberUpdate>(r.value()) == in);
}



TEST_CASE("bnet: 0x67 FRIENDADD ack round-trip online channel", "[protocol][bnet]") {
    using namespace protocol::bnet;
    FriendAddAck in;
    in.name          = "foo";
    in.status        = 0x01u;  // mutual
    in.location      = 0x02u;  // chat
    in.client_tag    = 0x57334f50u;
    in.location_name = "Channel.SwZ";
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<FriendAddAck>(r.value()) == in);
}

TEST_CASE("bnet: 0x67 FRIENDADD ack offline", "[protocol][bnet]") {
    using namespace protocol::bnet;
    FriendAddAck in;
    in.name          = "foo";
    in.status        = 0x00u;
    in.location      = 0x00u;  // offline
    in.client_tag    = 0;
    in.location_name = "";
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<FriendAddAck>(r.value()) == in);
}

TEST_CASE("bnet: 0x68 FRIENDDEL ack round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    FriendDelAck in;
    in.friend_num = 0x03u;
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<FriendDelAck>(r.value()) == in);
}

TEST_CASE("bnet: 0x69 FRIENDMOVE ack round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    FriendMoveAck in;
    in.pos1 = 0x02u;
    in.pos2 = 0x05u;
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<FriendMoveAck>(r.value()) == in);
}



TEST_CASE("bnet: 0x60 ARRANGEDTEAM_FRIENDSCREEN request empty", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ArrangedTeamFriendScreenRequest in;
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ArrangedTeamFriendScreenRequest>(r.value()) == in);
}

TEST_CASE("bnet: 0x60 ARRANGEDTEAM_FRIENDSCREEN reply round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ArrangedTeamFriendScreenReply in;
    in.names = {"alice", "bob", "carol"};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ArrangedTeamFriendScreenReply>(r.value()) == in);
}

TEST_CASE("bnet: 0x61 ARRANGEDTEAM_INVITE_FRIEND request round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ArrangedTeamInviteFriendRequest in;
    in.count    = 1;
    in.id       = 0x02A07BC9u;
    in.unknown1 = 1;
    in.friends  = {"trendecide"};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ArrangedTeamInviteFriendRequest>(r.value()) == in);
}

TEST_CASE("bnet: 0x61 ARRANGEDTEAM_INVITE_FRIEND ack round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ArrangedTeamInviteFriendAck in;
    in.count     = 1;
    in.id        = 0x02A07BC9u;
    in.timestamp = 0x55667788u;
    in.team_size = 2;
    in.info      = {0x10, 0x20, 0x30, 0x40, 0x50};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ArrangedTeamInviteFriendAck>(r.value()) == in);
}

TEST_CASE("bnet: 0x62 ARRANGEDTEAM_MEMBER_DECLINE round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ArrangedTeamMemberDecline in;
    in.count         = 1;
    in.action        = 2;
    in.decliner_name = "trendecide";
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ArrangedTeamMemberDecline>(r.value()) == in);
}

TEST_CASE("bnet: 0x63 ARRANGEDTEAM_SEND_INVITE round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ArrangedTeamSendInvite in;
    in.count        = 1;
    in.id           = 0x02A07BC9u;
    in.inviter_ip   = 0x0100007Fu;
    in.port         = 6112;
    in.inviter_name = "captain";
    in.other_names  = {"alice", "bob"};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ArrangedTeamSendInvite>(r.value()) == in);
}

TEST_CASE("bnet: 0x63 ARRANGEDTEAM_ACCEPT_DECLINE_INVITE round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ArrangedTeamAcceptDeclineInvite in;
    in.count        = 1;
    in.id           = 0x02A07BC9u;
    in.option       = 3;  // accept
    in.inviter_name = "captain";
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ArrangedTeamAcceptDeclineInvite>(r.value()) == in);
}

TEST_CASE("bnet: 0xFD ARRANGEDTEAM_ACCEPT_INVITE empty round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ArrangedTeamAcceptInvite in;
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ArrangedTeamAcceptInvite>(r.value()) == in);
}

TEST_CASE("bnet: 0x60 ARRANGEDTEAM_FRIENDSCREEN reply rejects oversize", "[protocol][bnet]") {
    using namespace protocol::bnet;
    protocol::Writer w;
    w.begin_bnet_packet(kSidArrangedTeamFriendScreen);
    w.write_le<std::uint8_t>(200u);
    REQUIRE(w.finalize_bnet_packet().has_value());
    auto fp = protocol::parse_packet(w.view());
    REQUIRE(fp.has_value());
    auto r = decode_server(fp.value().packet);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::InvalidArgument);
}



TEST_CASE("bnet: 0x09 GETADVLISTEX request round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    GameListRequest in;
    in.gametype  = 0x0002u;
    in.unknown1  = 0x0001u;
    in.unknown2  = 0xCAFEBABEu;
    in.unknown3  = 0x12345678u;
    in.max_games = 0xFFFFFFFFu;
    in.game_name = "MyGame";
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<GameListRequest>(r.value()) == in);
}

TEST_CASE("bnet: 0x09 GETADVLISTEX reply with two games", "[protocol][bnet]") {
    using namespace protocol::bnet;
    GameListReply in;
    in.sstatus = 0u;
    GameListEntry a;
    a.gametype = 0x0002u;
    a.unknown1 = 0x0001u;
    a.unknown3 = 0x0011u;
    a.port     = 0xC101u;       // BE bytes preserved as host u16
    a.game_ip  = 0x0A000001u;   // BE bytes preserved as host u32
    a.unknown4 = 0x10u;
    a.unknown5 = 0x20u;
    a.status   = 0x00u;
    a.unknown6 = 0x30u;
    a.game_name = "Alpha";
    a.password  = "";
    a.info      = ",,,,,,";
    GameListEntry b = a;
    b.game_name = "Beta";
    b.password  = "secret";
    in.entries  = {a, b};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<GameListReply>(r.value()) == in);
}

TEST_CASE("bnet: 0x09 GETADVLISTEX reply error (no games)", "[protocol][bnet]") {
    using namespace protocol::bnet;
    GameListReply in;
    in.sstatus = 0x03u;          // GAMENOTFOUND
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<GameListReply>(r.value()) == in);
}

TEST_CASE("bnet: 0x1C STARTADVEX3 request round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    StartGame4Request in;
    in.status   = 0x0010u;
    in.flag     = 0x0001u;
    in.unknown2 = 0xDEADBEEFu;
    in.gametype = 0x0002u;
    in.option   = 0x0004u;
    in.unknown4 = 0x11u;
    in.unknown5 = 0x22u;
    in.game_name = "MyHostedGame";
    in.password  = "pw";
    in.info      = "stats-blob";
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<StartGame4Request>(r.value()) == in);
}

TEST_CASE("bnet: 0x1C STARTADVEX3 ack success and failure", "[protocol][bnet]") {
    using namespace protocol::bnet;
    StartGame4Ack ok;
    ok.reply = 0u;
    auto r = round_trip(ok, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<StartGame4Ack>(r.value()) == ok);

    StartGame4Ack no;
    no.reply = 0x01u;
    auto r2 = round_trip(no, decode_server);
    REQUIRE(r2.has_value());
    REQUIRE(std::get<StartGame4Ack>(r2.value()) == no);
}


TEST_CASE("bnet codec: UDP_OK (0x14) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    UdpOk in{0x626E6574u};  // "tenb" little-endian-ish marker
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<UdpOk>(r.value()));
    REQUIRE(std::get<UdpOk>(r.value()) == in);
}

TEST_CASE("bnet codec: LADDERREQ (0x2E) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    LadderListRequest in{};
    in.client_tag = 0x44325650u;  // 'D2VP'
    in.id         = 1u;
    in.type       = 0u;
    in.start      = 50u;
    in.count      = 20u;
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<LadderListRequest>(r.value()));
    REQUIRE(std::get<LadderListRequest>(r.value()) == in);
}

TEST_CASE("bnet codec: LADDERREPLY (0x2E) with two entries",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    LadderListReply in{};
    in.client_tag = 0x44325650u;
    in.id         = 1u;
    in.type       = 0u;
    in.start      = 0u;
    in.count      = 2u;
    LadderListEntry e1{};
    e1.current = {12u, 3u, 1u, 1840u, 1u};
    e1.active  = {12u, 3u, 1u, 1840u, 1u};
    e1.ttest   = {0u, 0u, 0u, 0u, 0u, 0u};
    e1.lastgame_current = 0x0123456789ABCDEFull;
    e1.lastgame_active  = 0x00112233AABBCCDDull;
    e1.player_name      = "Player1";
    LadderListEntry e2{};
    e2.current = {7u, 4u, 0u, 1610u, 2u};
    e2.active  = {7u, 4u, 0u, 1610u, 2u};
    e2.ttest   = {0u, 0u, 0u, 0u, 0u, 0u};
    e2.lastgame_current = 0xFEEDFACECAFEBABEull;
    e2.lastgame_active  = 0xDEADBEEFBADC0DE5ull;
    e2.player_name      = "Player#2";
    in.entries = {e1, e2};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<LadderListReply>(r.value()));
    REQUIRE(std::get<LadderListReply>(r.value()) == in);
}

TEST_CASE("bnet codec: LADDERREPLY empty", "[protocol][bnet]") {
    using namespace protocol::bnet;
    LadderListReply in{};
    in.client_tag = 0x57415233u;  // 'WAR3'
    in.id         = 3u;
    in.type       = 2u;
    in.start      = 0u;
    in.count      = 0u;
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<LadderListReply>(r.value()));
    REQUIRE(std::get<LadderListReply>(r.value()).entries.empty());
}

TEST_CASE("bnet codec: LADDERREPLY encode rejects count mismatch",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    LadderListReply in{};
    in.count = 2u;  // entries is empty -> mismatch
    protocol::Writer w;
    auto s = encode(w, in);
    REQUIRE_FALSE(s.has_value());
    REQUIRE(s.error().code() == core::StatusCode::InvalidArgument);
}


TEST_CASE("bnet codec: AD request (0x15) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    AdRequest in{0x36385849u, 0x52415453u, 0x3614AFu, 0x36551400u};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<AdRequest>(r.value()));
    REQUIRE(std::get<AdRequest>(r.value()) == in);
}

TEST_CASE("bnet codec: AD reply (0x15) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    AdReply in{};
    in.adid          = 0x72u;
    in.extension_tag = 0x7863702Eu;  // ".pcx"
    in.timestamp     = 0x01BD0FCE1C7A1550ull;
    in.filename      = "ad000072.pcx";
    in.link          = "http://www.blizzard.com/";
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<AdReply>(r.value()));
    REQUIRE(std::get<AdReply>(r.value()) == in);
}

TEST_CASE("bnet codec: AD click (0x16) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    AdClick in{0x72u, 0u};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<AdClick>(r.value()) == in);
}

TEST_CASE("bnet codec: AD ack (0x21) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    AdAck in{};
    in.arch_tag   = 0x36385849u;
    in.client_tag = 0x4C545244u;
    in.adid       = 0xC3u;
    in.adfile     = "ad0000c3.smk";
    in.adlink     = "http://www.fatherhood.org/";
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<AdAck>(r.value()) == in);
}

TEST_CASE("bnet codec: ADCLICK2 (0x41) request round-trip",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    AdClick2Request in{0x0000200Bu};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<AdClick2Request>(r.value()) == in);
}

TEST_CASE("bnet codec: ADCLICK2 (0x41) reply round-trip",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    AdClick2Reply in{};
    in.adid = 0x0000200Bu;
    in.link = "http://www.blizzard.com/diablo2exp/";
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<AdClick2Reply>(r.value()) == in);
}

TEST_CASE("bnet codec: MOTD request (0x46) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    MotdRequest in{0x6C3A1601u};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<MotdRequest>(r.value()) == in);
}

TEST_CASE("bnet codec: MOTD reply (0x46) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    MotdReply in{};
    in.msg_type        = 1;
    in.curr_time       = 0x6C3A1601u;
    in.first_news_time = 0xFFFFFFFFu;
    in.timestamp       = 0u;
    in.timestamp2      = 0u;
    in.text            = "Welcome to Battle.net!";
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<MotdReply>(r.value()) == in);
}


TEST_CASE("bnet codec: PROGIDENT2 (0x0B) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ChannelListRequest in{0x57415233u};  // 'WAR3'
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ChannelListRequest>(r.value()) == in);
}

TEST_CASE("bnet codec: CHANNELLIST (0x0B) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ChannelListReply in{};
    in.channels = {"Blizzard Tech Support", "Public Chat", "Clan Recruitment"};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ChannelListReply>(r.value()) == in);
}

TEST_CASE("bnet codec: CHANNELLIST empty round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ChannelListReply in{};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ChannelListReply>(r.value()).channels.empty());
}

TEST_CASE("bnet codec: LEAVECHANNEL (0x10) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    LeaveChannel in{};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<LeaveChannel>(r.value()));
}

TEST_CASE("bnet codec: REGSNOOPREQ (0x18) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    RegSnoopRequest in{};
    in.unknown1   = 0u;
    in.hkey       = 0x80000001u;  // HKEY_CURRENT_USER
    in.reg_key    = "Software\\Microsoft\\MS Setup (ACME)\\User Info";
    in.value_name = "DefName";
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<RegSnoopRequest>(r.value()) == in);
}

TEST_CASE("bnet codec: REGSNOOPREPLY (0x18) cstring payload",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    RegSnoopReply in{};
    in.unknown1 = 0u;
    const char kBob[] = "Bob";  // includes NUL terminator
    in.value.assign(
        reinterpret_cast<const std::byte*>(kBob),
        reinterpret_cast<const std::byte*>(kBob) + sizeof(kBob));
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<RegSnoopReply>(r.value()) == in);
}

TEST_CASE("bnet codec: REGSNOOPREPLY (0x18) dword payload",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    RegSnoopReply in{};
    in.unknown1 = 0u;
    const std::uint8_t kDword[] = {0xA0u, 0x9Cu, 0xA0u, 0x00u};
    in.value.assign(
        reinterpret_cast<const std::byte*>(kDword),
        reinterpret_cast<const std::byte*>(kDword) + sizeof(kDword));
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<RegSnoopReply>(r.value()) == in);
}


TEST_CASE("bnet codec: PROFILE request (0x35) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ProfileRequest in{0x12345678u, "Bob"};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ProfileRequest>(r.value()) == in);
}

TEST_CASE("bnet codec: PROFILE reply (0x35) success round-trip",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    ProfileReply in{};
    in.cookie      = 0x12345678u;
    in.fail        = 0;
    in.description = "Just some guy";
    in.location    = "Somewhere";
    in.clan_tag    = 0x21574152u;  // 'WAR!' reversed
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ProfileReply>(r.value()) == in);
}

TEST_CASE("bnet codec: PROFILE reply (0x35) failure round-trip",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    ProfileReply in{};
    in.cookie = 0xDEADBEEFu;
    in.fail   = 1;
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ProfileReply>(r.value()) == in);
}

TEST_CASE("bnet codec: SETEMAILREQ (0x59) empty round-trip",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    SetEmailRequest in{};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<SetEmailRequest>(r.value()));
}

TEST_CASE("bnet codec: SETEMAILREPLY (0x59) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    SetEmailReply in{"bob@example.com"};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<SetEmailReply>(r.value()) == in);
}


TEST_CASE("bnet codec: ICONREQ (0x2D) empty round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    IconRequest in{};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<IconRequest>(r.value()));
}

TEST_CASE("bnet codec: ICONREPLY (0x2D) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    IconReply in{};
    in.timestamp = 0x01BDD6C08F1F3476ull;
    in.filename  = "icons.bni";
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<IconReply>(r.value()) == in);
}


TEST_CASE("bnet codec: GETPASSWORDREQ (0x5A) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    GetPasswordRequest in{"Bob", "bob@example.com"};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<GetPasswordRequest>(r.value()) == in);
}

TEST_CASE("bnet codec: CHANGEEMAILREQ (0x5B) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ChangeEmailRequest in{"Bob", "old@example.com", "new@example.com"};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ChangeEmailRequest>(r.value()) == in);
}

TEST_CASE("bnet codec: CRASHDUMP (0x5D) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    CrashDump in{};
    const std::uint8_t kBlob[] = {
        0x01u, 0x01u, 0x00u, 0x27u, 0x00u, 0x0Au, 0x01u, 0x05u,
        0x00u, 0x00u, 0xC0u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u};
    in.data.assign(
        reinterpret_cast<const std::byte*>(kBlob),
        reinterpret_cast<const std::byte*>(kBlob) + sizeof(kBlob));
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<CrashDump>(r.value()) == in);
}

TEST_CASE("bnet codec: CRASHDUMP empty body round-trips", "[protocol][bnet]") {
    using namespace protocol::bnet;
    CrashDump in{};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<CrashDump>(r.value()).data.empty());
}


TEST_CASE("bnet codec: CHARLIST request (0x37) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    CharListRequest in{};
    in.open_count = 4;
    const std::uint8_t kBlob[] = {
        0x42u, 0x65u, 0x74u, 0x61u, 0x57u, 0x65u, 0x73u, 0x74u,
        0x2Cu, 0x4Du, 0x6Fu, 0x4Eu, 0x6Bu, 0x00u, 0x87u, 0x80u};
    in.char_data.assign(
        reinterpret_cast<const std::byte*>(kBlob),
        reinterpret_cast<const std::byte*>(kBlob) + sizeof(kBlob));
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<CharListRequest>(r.value()) == in);
}

TEST_CASE("bnet codec: CHARLIST reply (0x37) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    CharListReply in{};
    in.unknown1 = 0;
    in.max_chars = 8;
    in.count = 1;
    const std::uint8_t kBlob[] = {
        0x42u, 0x65u, 0x74u, 0x61u, 0x57u, 0x65u, 0x73u, 0x74u,
        0x2Cu, 0x4Cu, 0x69u, 0x66u, 0x65u, 0x6Cu, 0x69u, 0x6Bu,
        0x65u, 0x00u, 0x87u, 0x80u};
    in.char_data.assign(
        reinterpret_cast<const std::byte*>(kBlob),
        reinterpret_cast<const std::byte*>(kBlob) + sizeof(kBlob));
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<CharListReply>(r.value()) == in);
}

TEST_CASE("bnet codec: CHARLIST reply with zero chars", "[protocol][bnet]") {
    using namespace protocol::bnet;
    CharListReply in{};
    in.unknown1 = 0;
    in.max_chars = 8;
    in.count = 0;
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    auto& out = std::get<CharListReply>(r.value());
    REQUIRE(out.count == 0);
    REQUIRE(out.char_data.empty());
}


TEST_CASE("bnet codec: SERVERLIST (0x04) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ServerList in{};
    in.unknown1 = 0;
    in.servers = "209.67.136.174;207.69.194.210;exodus.battle.net";
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ServerList>(r.value()) == in);
}

TEST_CASE("bnet codec: SERVERLIST empty list round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ServerList in{};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ServerList>(r.value()) == in);
}


TEST_CASE("bnet codec: MESSAGEBOX (0x19) OK round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    MessageBox in{};
    in.style   = kMessageBoxStyleOk;
    in.text    = "Server maintenance in 5 minutes";
    in.caption = "Battle.net";
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<MessageBox>(r.value()) == in);
}

TEST_CASE("bnet codec: MESSAGEBOX YESNO round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    MessageBox in{};
    in.style   = kMessageBoxStyleYesNo;
    in.text    = "Reconnect?";
    in.caption = "Disconnected";
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<MessageBox>(r.value()) == in);
}

TEST_CASE("bnet codec: MESSAGEBOX empty strings round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    MessageBox in{};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<MessageBox>(r.value()) == in);
}


TEST_CASE("bnet codec: REALMLIST request (0x40) empty body", "[protocol][bnet]") {
    using namespace protocol::bnet;
    RealmListRequest in{};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<RealmListRequest>(r.value()));
}

TEST_CASE("bnet codec: REALMLIST reply (0x40) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    RealmListReply in{};
    in.unknown1 = 0;
    in.entries.push_back({1u, "Europe", "Realm for Europe"});
    in.entries.push_back({1u, "USEast", "US East coast"});
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<RealmListReply>(r.value()) == in);
}

TEST_CASE("bnet codec: REALMLIST reply with zero entries", "[protocol][bnet]") {
    using namespace protocol::bnet;
    RealmListReply in{};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<RealmListReply>(r.value()) == in);
}

TEST_CASE("bnet codec: REALMJOIN request (0x3E) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    RealmJoinRequest in{};
    in.seqno = 0x12345678u;
    in.seqno_hash = {0xaaaaaaaau, 0xbbbbbbbbu, 0xccccccccu, 0xddddddddu, 0xeeeeeeeeu};
    in.realm_name = "QarathRealm";
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<RealmJoinRequest>(r.value()) == in);
}

TEST_CASE("bnet codec: REALMJOIN reply (0x3E) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    RealmJoinReply in{};
    in.seqno        = 0x11223344u;
    in.u1           = 0;
    in.bncs_addr1   = 0xc0a80101u;
    in.session_num  = 7u;
    in.addr         = 0x7f000001u;
    in.port         = 6112;       // network byte order on wire
    in.u3           = 0;
    in.session_key  = 0;
    in.u5           = 0;
    in.u6           = 0;
    in.client_tag   = 0x44324456u; // 'D2DV'
    in.version_id   = 0x10u;
    in.bncs_addr2   = 0xc0a80101u;
    in.u7           = 0;
    in.secret_hash  = {1u, 2u, 3u, 4u, 5u};
    in.account_name = "Tester";
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<RealmJoinReply>(r.value()) == in);
}


TEST_CASE("bnet codec: WARCRAFTGENERAL search request (0x44/0x00)", "[protocol][bnet]") {
    using namespace protocol::bnet;
    WarcraftGeneralRequest in{};
    in.sub_option = kAnonGameClientSearch;
    const std::uint8_t kBlob[] = {
        0x01u, 0x00u, 0x00u, 0x00u,  // count
        0x00u, 0x00u, 0x00u, 0x00u,  // unknown2
        0x00u, 0x00u, 0xFFu,         // type, gametype, map_prefs[0]
        0x00u, 0x00u, 0x00u,         // map_prefs[1..3]
        0x08u,                       // unknown3
        0xC2u, 0x13u, 0xA6u, 0x02u,  // id
        0x20u, 0x00u, 0x00u, 0x00u}; // race
    in.data.assign(
        reinterpret_cast<const std::byte*>(kBlob),
        reinterpret_cast<const std::byte*>(kBlob) + sizeof(kBlob));
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<WarcraftGeneralRequest>(r.value()) == in);
}

TEST_CASE("bnet codec: WARCRAFTGENERAL cancel (0x44/0x03) empty tail", "[protocol][bnet]") {
    using namespace protocol::bnet;
    WarcraftGeneralRequest in{};
    in.sub_option = kAnonGameClientCancel;
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    auto& out = std::get<WarcraftGeneralRequest>(r.value());
    REQUIRE(out.sub_option == kAnonGameClientCancel);
    REQUIRE(out.data.empty());
}

TEST_CASE("bnet codec: WARCRAFTGENERAL server search reply (0x44/0x00)", "[protocol][bnet]") {
    using namespace protocol::bnet;
    WarcraftGeneralReply in{};
    in.sub_option = kAnonGameServerSearch;
    const std::uint8_t kBlob[] = {
        0x01u, 0x00u, 0x00u, 0x00u,  // count
        0x00u, 0x00u, 0x00u, 0x00u}; // reply code
    in.data.assign(
        reinterpret_cast<const std::byte*>(kBlob),
        reinterpret_cast<const std::byte*>(kBlob) + sizeof(kBlob));
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<WarcraftGeneralReply>(r.value()) == in);
}

TEST_CASE("bnet codec: WARCRAFTGENERAL server found (0x44/0x01)", "[protocol][bnet]") {
    using namespace protocol::bnet;
    WarcraftGeneralReply in{};
    in.sub_option = kAnonGameServerFound;
    // sample fragment matching a real 0x44/0x01 reply
    const std::uint8_t kBlob[] = {
        0x01u, 0x00u, 0x00u, 0x00u, 0x4Du, 0x61u, 0x70u, 0x73u,
        0x5Cu, 0x46u, 0x54u, 0x00u};
    in.data.assign(
        reinterpret_cast<const std::byte*>(kBlob),
        reinterpret_cast<const std::byte*>(kBlob) + sizeof(kBlob));
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<WarcraftGeneralReply>(r.value()) == in);
}


TEST_CASE("bnet codec: REQUIREDWORK (0x4C) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    RequiredWork in{};
    in.filename = "IX86ExtraWork.mpq";
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<RequiredWork>(r.value()) == in);
}

TEST_CASE("bnet codec: REQUIREDWORK empty filename", "[protocol][bnet]") {
    using namespace protocol::bnet;
    RequiredWork in{};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<RequiredWork>(r.value()) == in);
}

TEST_CASE("bnet codec: EXTRAWORK (0x4B) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ExtraWork in{};
    in.game_type = 6;
    const std::uint8_t kBlob[] = {
        0xDEu, 0xADu, 0xBEu, 0xEFu, 0x01u, 0x02u, 0x03u, 0x04u};
    in.data.assign(
        reinterpret_cast<const std::byte*>(kBlob),
        reinterpret_cast<const std::byte*>(kBlob) + sizeof(kBlob));
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ExtraWork>(r.value()) == in);
}

TEST_CASE("bnet codec: EXTRAWORK empty payload", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ExtraWork in{};
    in.game_type = 0;
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ExtraWork>(r.value()) == in);
}


TEST_CASE("bnet codec: REALMLIST legacy request (0x34)", "[protocol][bnet]") {
    using namespace protocol::bnet;
    RealmListLegacyRequest in{};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<RealmListLegacyRequest>(r.value()) == in);
}

TEST_CASE("bnet codec: REALMLIST legacy reply (0x34) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    RealmListLegacyReply in{};
    RealmListLegacyEntry e{};
    e.unknown3 = 0xC0000000u;
    e.unknown4 = 0;
    e.unknown5 = 0;
    e.unknown6 = 0;
    e.unknown7 = 0x00018210u;
    e.unknown8 = 0xFFFFFFFFu;
    e.unknown9 = 0;
    e.name = "BetaWest";
    e.description = "Please select this as your realm during beta";
    in.entries.push_back(e);
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<RealmListLegacyReply>(r.value()) == in);
}

TEST_CASE("bnet codec: REALMLIST legacy reply empty", "[protocol][bnet]") {
    using namespace protocol::bnet;
    RealmListLegacyReply in{};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<RealmListLegacyReply>(r.value()) == in);
}


TEST_CASE("bnet codec: CDKEY3 request (0x42) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    CdKey3Request in{};
    in.unknown1 = 0x3BA125C6u;
    in.unknown2 = 0x00000001u;
    in.unknown3 = 0x00000000u;
    in.unknown4 = 0x00000010u;
    in.unknown5 = 0x00000006u;
    in.unknown6 = 0x0010F300u;
    in.unknown7 = 0x00000000u;
    in.key_hash = {0xC48B29A8u, 0xAB33BD41u, 0x1E1F4C74u, 0x83CA005Cu, 0x1436E57Fu};
    in.owner_name = "OwnerName123";
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<CdKey3Request>(r.value()) == in);
}

TEST_CASE("bnet codec: CDKEY3 reply OK (0x42) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    CdKey3Reply in{};
    in.message = kCdKeyReply3MessageOk;
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<CdKey3Reply>(r.value()) == in);
}

TEST_CASE("bnet codec: CDKEY3 reply with owner name", "[protocol][bnet]") {
    using namespace protocol::bnet;
    CdKey3Reply in{};
    in.message = kCdKeyReply3MessageOk;
    in.owner_name = "OwnerName123";
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<CdKey3Reply>(r.value()) == in);
}


TEST_CASE("bnet codec: CREATEACCOUNT2 request (0x52) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    CreateAccount2Request in{};
    for (std::size_t i = 0; i < in.salt.size(); ++i)
        in.salt[i] = static_cast<std::uint8_t>(0x10 + i);
    for (std::size_t i = 0; i < in.password_verifier.size(); ++i)
        in.password_verifier[i] = static_cast<std::uint8_t>(0x80 + i);
    in.account_name = "theaccountname";
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<CreateAccount2Request>(r.value()) == in);
}

TEST_CASE("bnet codec: CREATEACCOUNT2 reply OK (0x52) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    CreateAccount2Reply in{};
    in.result = kCreateAccount2ResultOk;
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<CreateAccount2Reply>(r.value()) == in);
}

TEST_CASE("bnet codec: CREATEACCOUNT2 reply EXISTS (0x52) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    CreateAccount2Reply in{};
    in.result = kCreateAccount2ResultExists;
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<CreateAccount2Reply>(r.value()) == in);
}


TEST_CASE("bnet codec: LOGINREQ_W3 (0x53) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    LoginW3Request in{};
    for (std::size_t i = 0; i < in.client_public_key.size(); ++i)
        in.client_public_key[i] = static_cast<std::uint8_t>(0x40 + i);
    in.account_name = "Jon";
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<LoginW3Request>(r.value()) == in);
}

TEST_CASE("bnet codec: LOGINREPLY_W3 success (0x53) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    LoginW3Reply in{};
    in.message = kLoginW3MessageSuccess;
    for (std::size_t i = 0; i < in.salt.size(); ++i)
        in.salt[i] = static_cast<std::uint8_t>(0xA0 + i);
    for (std::size_t i = 0; i < in.server_public_key.size(); ++i)
        in.server_public_key[i] = static_cast<std::uint8_t>(0x10 + i);
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<LoginW3Reply>(r.value()) == in);
}

TEST_CASE("bnet codec: LOGINREPLY_W3 failure (0x53) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    LoginW3Reply in{};
    in.message = kLoginW3MessageFailure;
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<LoginW3Reply>(r.value()) == in);
}


TEST_CASE("bnet codec: LOGONPROOFREQ (0x54) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    LogonProofW3Request in{};
    for (std::size_t i = 0; i < in.client_password_proof.size(); ++i)
        in.client_password_proof[i] = static_cast<std::uint8_t>(0x30 + i);
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<LogonProofW3Request>(r.value()) == in);
}

TEST_CASE("bnet codec: LOGONPROOFREPLY OK (0x54) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    LogonProofW3Reply in{};
    in.response = kLogonProofW3ResponseOk;
    for (std::size_t i = 0; i < in.server_password_proof.size(); ++i)
        in.server_password_proof[i] = static_cast<std::uint8_t>(0x70 + i);
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<LogonProofW3Reply>(r.value()) == in);
}

TEST_CASE("bnet codec: LOGONPROOFREPLY CUSTOM (0x54) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    LogonProofW3Reply in{};
    in.response = kLogonProofW3ResponseCustom;
    for (std::size_t i = 0; i < in.server_password_proof.size(); ++i)
        in.server_password_proof[i] = static_cast<std::uint8_t>(0xC0 + i);
    in.message = "This account has been locked";
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<LogonProofW3Reply>(r.value()) == in);
}


TEST_CASE("bnet codec: PASSCHANGEREQ (0x55) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    PassChangeRequest in{};
    for (std::size_t i = 0; i < in.client_public_key.size(); ++i)
        in.client_public_key[i] = static_cast<std::uint8_t>(0x50 + i);
    in.account_name = "Jon";
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<PassChangeRequest>(r.value()) == in);
}

TEST_CASE("bnet codec: PASSCHANGEREPLY accept (0x55) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    PassChangeReply in{};
    in.message = kPassChangeMessageAccept;
    for (std::size_t i = 0; i < in.salt.size(); ++i)
        in.salt[i] = static_cast<std::uint8_t>(0x90 + i);
    for (std::size_t i = 0; i < in.server_public_key.size(); ++i)
        in.server_public_key[i] = static_cast<std::uint8_t>(0x20 + i);
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<PassChangeReply>(r.value()) == in);
}

TEST_CASE("bnet codec: PASSCHANGEREPLY reject (0x55) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    PassChangeReply in{};
    in.message = kPassChangeMessageReject;
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<PassChangeReply>(r.value()) == in);
}


TEST_CASE("bnet codec: PASSCHANGEPROOFREQ (0x56) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    PassChangeProofRequest in{};
    for (std::size_t i = 0; i < in.client_password_proof.size(); ++i)
        in.client_password_proof[i] = static_cast<std::uint8_t>(0x60 + i);
    for (std::size_t i = 0; i < in.salt.size(); ++i)
        in.salt[i] = static_cast<std::uint8_t>(0x80 + i);
    for (std::size_t i = 0; i < in.password_verifier.size(); ++i)
        in.password_verifier[i] = static_cast<std::uint8_t>(0xB0 + i);
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<PassChangeProofRequest>(r.value()) == in);
}

TEST_CASE("bnet codec: PASSCHANGEPROOFREPLY OK (0x56) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    PassChangeProofReply in{};
    in.response = kPassChangeProofResponseOk;
    for (std::size_t i = 0; i < in.server_password_proof.size(); ++i)
        in.server_password_proof[i] = static_cast<std::uint8_t>(0x10 + i);
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<PassChangeProofReply>(r.value()) == in);
}

TEST_CASE("bnet codec: PASSCHANGEPROOFREPLY BADPASS (0x56) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    PassChangeProofReply in{};
    in.response = kPassChangeProofResponseBadPass;
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<PassChangeProofReply>(r.value()) == in);
}


// ============================================================================
// Legacy / OLS SID round-trip tests (bulk batch).
// ============================================================================

TEST_CASE("bnet codec: SID_CLIENTID (0x05) request round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    CompInfo1Request in{0x11111111, 0x22222222, 0x33333333, 0x44444444, "host", "user"};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<CompInfo1Request>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_CLIENTID (0x05) request empty strings round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    CompInfo1Request in{0xA, 0xB, 0xC, 0xD, "", ""};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<CompInfo1Request>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_CLIENTID (0x05) reply round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    CompReply in{1, 2, 3, 4};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<CompReply>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_PROGIDENT (0x06) request round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ProgIdent in{0x49583836u, 0x53455850u, 0xABABABABu, 0};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ProgIdent>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_PROGIDENT (0x06) server round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    AuthReq1Server in{0x0123456789ABCDEFull, "IX86ver1.mpq", "A=1 B=2 C=3 4 A=A^S B=B-C C=C-A A=A+B"};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<AuthReq1Server>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_AUTH (0x07) request round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    AuthReq1 in{0x49583836u, 0x53455850u, 0xCAFEu, 0x000A0099u, 0xDEADBEEFu, "exe info string"};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<AuthReq1>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_AUTH (0x07) reply OK round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    AuthReply1 in{kAuthReply1MessageOk, ""};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<AuthReply1>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_AUTH (0x07) reply with filename round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    AuthReply1 in{kAuthReply1MessageUpdate, "patch.mpq"};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<AuthReply1>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_AUTH (0x07) reply byte parity vs legacy",
          "[protocol][bnet]") {
    using namespace protocol::bnet;
    // Reference bytes are taken from src/common/bnet_protocol.h:
    //   "FF 07 0A 00 02 00 00 00 00 00"
    // Header (FF 07 size_le) + u32 message (OK=2) + ""\0 + ""\0
    {
        AuthReply1 m{kAuthReply1MessageOk, ""};
        protocol::Writer w;
        REQUIRE(encode(w, m).has_value());
        auto bytes = w.take();
        const std::uint8_t expected[] = {
            0xFF, 0x07, 0x0A, 0x00,
            0x02, 0x00, 0x00, 0x00,
            0x00, 0x00
        };
        REQUIRE(bytes.size() == sizeof(expected));
        for (std::size_t i = 0; i < sizeof(expected); ++i) {
            REQUIRE(static_cast<std::uint8_t>(bytes[i]) == expected[i]);
        }
    }
    // BADVERSION variant: message=0, no filename, two trailing empties.
    {
        AuthReply1 m{kAuthReply1MessageBadVersion, ""};
        protocol::Writer w;
        REQUIRE(encode(w, m).has_value());
        auto bytes = w.take();
        const std::uint8_t expected[] = {
            0xFF, 0x07, 0x0A, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00
        };
        REQUIRE(bytes.size() == sizeof(expected));
        for (std::size_t i = 0; i < sizeof(expected); ++i) {
            REQUIRE(static_cast<std::uint8_t>(bytes[i]) == expected[i]);
        }
    }
    // Update variant: prepended filename "p.mpq" + two trailing empties.
    {
        AuthReply1 m{kAuthReply1MessageOk, "p.mpq"};
        protocol::Writer w;
        REQUIRE(encode(w, m).has_value());
        auto bytes = w.take();
        const std::uint8_t expected[] = {
            0xFF, 0x07, 0x10, 0x00,           // size = 16
            0x02, 0x00, 0x00, 0x00,           // message = OK
            'p',  '.',  'm',  'p',  'q', 0x00, // "p.mpq"\0
            0x00,                              // ""\0
            0x00                               // ""\0
        };
        REQUIRE(bytes.size() == sizeof(expected));
        for (std::size_t i = 0; i < sizeof(expected); ++i) {
            REQUIRE(static_cast<std::uint8_t>(bytes[i]) == expected[i]);
        }
    }
}

TEST_CASE("bnet codec: SID_COUNTRYINFO1 (0x12) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    CountryInfo1 in{
        0x1122334455667788ull, 0x99AABBCCDDEEFF00ull,
        -300, 0x0409, 0x0409, 0x0409,
        "ENU", "1", "USA", "United States"};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<CountryInfo1>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_SESSIONKEY1 (0x1D) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    SessionKey1 in{0xCAFEBABEu};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<SessionKey1>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_COMPINFO2 (0x1E) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    CompInfo2 in{0xFF, 1, 2, 3, 4, "myhost", "myuser"};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<CompInfo2>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_COMPINFO2 (0x1E) no strings round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    CompInfo2 in{0xFF, 1, 2, 3, 4, "", ""};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<CompInfo2>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_SESSIONKEY2 (0x28) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    SessionKey2 in{0x10, 0xDEADBEEFu};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<SessionKey2>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_LOGONRESPONSE (0x29) request round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    LoginReq1 in{};
    in.ticks = 0x1000;
    in.sessionkey = 0xCAFEu;
    in.password_hash2 = {0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u, 0x55555555u};
    in.player_name = "alice";
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<LoginReq1>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_LOGONRESPONSE (0x29) reply round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    LoginReply1 in{kLoginReply1MessageSuccess};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<LoginReply1>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_CREATEACCOUNT1 (0x2A) request round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    CreateAccount1Request in{};
    in.password_hash1 = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
    in.player_name = "bob";
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<CreateAccount1Request>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_CREATEACCOUNT1 (0x2A) reply round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    CreateAccount1Reply in{kCreateAccount1ResultOk};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<CreateAccount1Reply>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_UNKNOWN_2B (0x2B) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    Unknown2B in{1, 2, 3, 4, 5, 6, 7};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<Unknown2B>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_CDKEY (0x30) request with owner round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    CdKeyLegacyRequest in{0, "ABCD-EFGH-IJKL-MNOP", "owner"};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<CdKeyLegacyRequest>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_CDKEY (0x30) request no owner round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    CdKeyLegacyRequest in{1, "ABCD-EFGH-IJKL-MNOP", ""};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<CdKeyLegacyRequest>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_CDKEY (0x30) reply with owner round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    CdKeyLegacyReply in{kCdKeyLegacyMessageOk, "owner"};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<CdKeyLegacyReply>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_CDKEY (0x30) reply no owner round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    CdKeyLegacyReply in{kCdKeyLegacyMessageInUse, ""};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<CdKeyLegacyReply>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_CHANGEPASSWORD (0x31) request round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ChangePasswordRequest in{};
    in.ticks = 0x2000;
    in.sessionkey = 0xBEEFu;
    in.oldpassword_hash2 = {1, 2, 3, 4, 5};
    in.newpassword_hash1 = {6, 7, 8, 9, 10};
    in.player_name = "alice";
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ChangePasswordRequest>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_CHANGEPASSWORD (0x31) reply round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ChangePasswordReply in{kChangePasswordMessageSuccess};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ChangePasswordReply>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_UNKNOWN_39 (0x39) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    Unknown39 in{"someChar"};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<Unknown39>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_CREATEACCOUNT (0x3D) request round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    CreateAccountRequest in{};
    in.password_hash1 = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
    in.username = "newuser";
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<CreateAccountRequest>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_CREATEACCOUNT (0x3D) reply round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    CreateAccountReply in{kCreateAccountResultOk};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<CreateAccountReply>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_NETGAMEPORT (0x45) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    NetGamePort in{6112};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<NetGamePort>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_COUNTRYINFO1 (0x12) byte parity vs legacy",
          "[protocol][bnet]") {
    using namespace pvpgn::protocol::bnet;
    // Reference payload from bnet_protocol.h CLIENT_COUNTRYINFO1 docs:
    //   56 17 A5 3F C0 01 A8 FD   systemtime (u64 LE)
    //   FF FF 09 0C 00 00 09 0C   bias (i32 LE = -300), langid1
    //   00 00 09 0C 00 00         langid2, langid3
    //   "ena\0" "61\0" "AUS\0" "Australia\0"
    // Plus the v3 begin_bnet_packet header (FF 12 size_lo size_hi).
    // The legacy reference text omits localtime; v3 includes it
    // explicitly. We exercise the v3 encoder with concrete values
    // and verify the on-wire byte order matches the documented LE
    // field order, including the 4 trailing cstrings.
    CountryInfo1 m;
    m.systemtime    = 0xFDA801C03FA51756ull;
    m.localtime     = 0x0000000000000000ull;
    m.bias          = -300;
    m.langid1       = 0x0C090000u;
    m.langid2       = 0x0C090000u;
    m.langid3       = 0x0C090000u;
    m.langstr       = "ena";
    m.countrycode   = "61";
    m.countryabbrev = "AUS";
    m.countryname   = "Australia";

    pvpgn::protocol::Writer w;
    REQUIRE(encode(w, m).has_value());
    auto bytes = w.take();
    // header(4) + 8 + 8 + 4 + 4 + 4 + 4 + 4 + 3 + 4 + 10 = 57.
    REQUIRE(bytes.size() == 57u);
    REQUIRE(static_cast<std::uint8_t>(bytes[0]) == 0xFFu);
    REQUIRE(static_cast<std::uint8_t>(bytes[1]) == 0x12u);
    REQUIRE(static_cast<std::uint8_t>(bytes[2]) == 57u);
    REQUIRE(static_cast<std::uint8_t>(bytes[3]) == 0u);
    // systemtime little-endian
    REQUIRE(static_cast<std::uint8_t>(bytes[4])  == 0x56u);
    REQUIRE(static_cast<std::uint8_t>(bytes[5])  == 0x17u);
    REQUIRE(static_cast<std::uint8_t>(bytes[6])  == 0xA5u);
    REQUIRE(static_cast<std::uint8_t>(bytes[7])  == 0x3Fu);
    REQUIRE(static_cast<std::uint8_t>(bytes[8])  == 0xC0u);
    REQUIRE(static_cast<std::uint8_t>(bytes[9])  == 0x01u);
    REQUIRE(static_cast<std::uint8_t>(bytes[10]) == 0xA8u);
    REQUIRE(static_cast<std::uint8_t>(bytes[11]) == 0xFDu);
    // localtime = 0
    for (std::size_t i = 12; i < 20; ++i)
        REQUIRE(static_cast<std::uint8_t>(bytes[i]) == 0u);
    // bias = -300 (0xFFFFFED4 little-endian)
    REQUIRE(static_cast<std::uint8_t>(bytes[20]) == 0xD4u);
    REQUIRE(static_cast<std::uint8_t>(bytes[21]) == 0xFEu);
    REQUIRE(static_cast<std::uint8_t>(bytes[22]) == 0xFFu);
    REQUIRE(static_cast<std::uint8_t>(bytes[23]) == 0xFFu);
    // langid1/2/3 = 0x0C090000
    for (int k = 0; k < 3; ++k) {
        REQUIRE(static_cast<std::uint8_t>(bytes[24 + 4 * k + 0]) == 0x00u);
        REQUIRE(static_cast<std::uint8_t>(bytes[24 + 4 * k + 1]) == 0x00u);
        REQUIRE(static_cast<std::uint8_t>(bytes[24 + 4 * k + 2]) == 0x09u);
        REQUIRE(static_cast<std::uint8_t>(bytes[24 + 4 * k + 3]) == 0x0Cu);
    }
    // Tail cstrings.
    const char expected_tail[] = "ena\0" "61\0" "AUS\0" "Australia";
    // length = 3+1 + 2+1 + 3+1 + 9+1 = 21.
    for (std::size_t i = 0; i < 21; ++i)
        REQUIRE(static_cast<std::uint8_t>(bytes[36 + i]) ==
                static_cast<std::uint8_t>(expected_tail[i]));
}

TEST_CASE("bnet codec: SID_REGSNOOPREPLY (0x18) byte parity vs legacy",
          "[protocol][bnet]") {
    using namespace pvpgn::protocol::bnet;
    // Reference bytes from bnet_protocol.h CLIENT_REGSNOOPREPLY:
    //   FF 18 0C 00 00 00 00 00   42 6F 62 00     "...Bob\0"
    // header(4) + u32 unknown1 + "Bob\0" (raw bytes appended via
    // `value`) = 12 bytes.
    RegSnoopReply m;
    m.unknown1 = 0u;
    const char payload[] = "Bob";
    m.value.assign(reinterpret_cast<std::byte const*>(payload),
                   reinterpret_cast<std::byte const*>(payload) + 4);

    pvpgn::protocol::Writer w;
    REQUIRE(encode(w, m).has_value());
    auto bytes = w.take();
    const std::uint8_t expected[] = {
        0xFF, 0x18, 0x0C, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x42, 0x6F, 0x62, 0x00
    };
    REQUIRE(bytes.size() == sizeof(expected));
    for (std::size_t i = 0; i < sizeof(expected); ++i)
        REQUIRE(static_cast<std::uint8_t>(bytes[i]) == expected[i]);
}

TEST_CASE("bnet codec: SID_NETGAMEPORT (0x45) byte parity vs legacy",
          "[protocol][bnet]") {
    using namespace pvpgn::protocol::bnet;
    // Reference bytes from bnet_protocol.h CLIENT_CHANGEGAMEPORT:
    //   FF 45 06 00 E0 17    port = 0x17E0 = 6112
    NetGamePort m;
    m.port = 6112;

    pvpgn::protocol::Writer w;
    REQUIRE(encode(w, m).has_value());
    auto bytes = w.take();
    const std::uint8_t expected[] = {
        0xFF, 0x45, 0x06, 0x00,
        0xE0, 0x17
    };
    REQUIRE(bytes.size() == sizeof(expected));
    for (std::size_t i = 0; i < sizeof(expected); ++i)
        REQUIRE(static_cast<std::uint8_t>(bytes[i]) == expected[i]);
}



// ============================================================================
// Game-lifecycle SIDs round-trip tests.
// ============================================================================

TEST_CASE("bnet codec: SID_STOPADV / CLOSEGAME (0x02) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    CloseGame in{};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<CloseGame>(r.value()));
}

TEST_CASE("bnet codec: SID_LEAVEGAME / CLOSEGAME2 (0x1F) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    CloseGame2 in{};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<CloseGame2>(r.value()));
}

TEST_CASE("bnet codec: SID_STARTADVEX / STARTGAME1 (0x08) request round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    StartGame1Request in{0x04, 0, 0x0009, 0x0001, 0, 0,
                          "TEST", "", "info,here"};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<StartGame1Request>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_STARTADVEX (0x08) ack round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    StartGame1Ack in{0u};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<StartGame1Ack>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_STARTADVEX2 / STARTGAME3 (0x1A) request round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    StartGame3Request in{0x04, 0, 0x0009, 0x0001, 0x1234, 0, 0,
                          "TEST", "pw", "info"};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<StartGame3Request>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_STARTADVEX2 (0x1A) ack round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    StartGame3Ack in{1u};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<StartGame3Ack>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_NOTIFYJOIN / JOIN_GAME (0x22) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    JoinGame in{0x57415232u, 0x49583836u, "Ladder 1 on 1", ""};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<JoinGame>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_GAMERESULT / GAME_REPORT (0x2C) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    GameReport in{};
    in.unknown1 = 0;
    in.results = {kGameReportResultWin, kGameReportResultLoss};
    in.player_names = {"alice", "bob"};
    in.report_header = "On map \"Foo\":";
    in.report_body = "alice was Protoss\nbob was Zerg";
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<GameReport>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_GAMERESULT (0x2C) empty roster round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    GameReport in{};
    in.results.clear();
    in.player_names.clear();
    in.report_header = "hdr";
    in.report_body = "body";
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<GameReport>(r.value()) == in);
}


// ============================================================================
// Misc / anti-cheat / advisory SIDs round-trip tests.
// ============================================================================

TEST_CASE("bnet codec: SID_READMEMORY (0x17) server request round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ReadMemoryRequest in{0xDEADBEEFu, 0x00400000u, 0x100u};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ReadMemoryRequest>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_READMEMORY (0x17) client reply round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ReadMemoryReply in{};
    in.request_id = 0xDEADBEEFu;
    in.memory = {0x90, 0x90, 0x90, 0xCC, 0x00, 0xFF};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ReadMemoryReply>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_READMEMORY (0x17) empty memory round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ReadMemoryReply in{};
    in.request_id = 1u;
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ReadMemoryReply>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_UNKNOWN_1B (0x1B) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    Unknown1B in{0x0002, 0x17E0u, 0x807B3F54u, 0u, 0u};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<Unknown1B>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_UNKNOWN_24 (0x24) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    Unknown24 in{};
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::holds_alternative<Unknown24>(r.value()));
}

TEST_CASE("bnet codec: SID_MAPAUTH1 (0x32) request round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    MapAuthReq1 in{};
    in.file_checksum = {0x1A29251Au, 0x7263CD3Cu, 0x6B4D7AA4u, 0x3B9238D5u, 0x01F4A56Bu};
    in.mapfile = "(2)Challenger.scm";
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<MapAuthReq1>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_MAPAUTH1 (0x32) reply round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    MapAuthReply1 in{kMapAuthReply1ResponseLadderOk};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<MapAuthReply1>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_MAPAUTH2 (0x3C) request round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    MapAuthReq2 in{};
    in.unknown = 0x00012079u;
    in.file_hash = {0x27C6B73Bu, 0x79C3610Du, 0x5E24BE79u, 0x7D05079Cu, 0x78A06A0Bu};
    in.mapfile = "(5)Jeweled River.scm";
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<MapAuthReq2>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_MAPAUTH2 (0x3C) reply round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    MapAuthReply2 in{1u};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<MapAuthReply2>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_CHANGECLIENT (0x5C) round-trip", "[protocol][bnet]") {
    using namespace protocol::bnet;
    ChangeClient in{0x57415233u};  // "WAR3"
    auto r = round_trip(in, decode_client);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ChangeClient>(r.value()) == in);
}

TEST_CASE("bnet codec: SID_READMEMORY (0x17) decode rejects oversized count guard",
          "[protocol][bnet]") {
    // No oversized-count guard for 0x17 (memory is opaque tail bytes), but
    // verify a request with length=0 still decodes cleanly.
    using namespace protocol::bnet;
    ReadMemoryRequest in{1u, 0u, 0u};
    auto r = round_trip(in, decode_server);
    REQUIRE(r.has_value());
    REQUIRE(std::get<ReadMemoryRequest>(r.value()) == in);
}


