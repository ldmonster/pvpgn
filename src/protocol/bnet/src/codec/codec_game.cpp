// SPDX-License-Identifier: GPL-2.0-or-later
// Auto-split from codec.cpp by scripts/dev/split_codec.py
// See plans/15-large-file-decomposition-detail.md §8 for rationale.
#include "codec_internal.h"

namespace pvpgn::protocol::bnet {
namespace detail {

// 4-byte spacer dword the GAMELISTREPLY emits between consecutive game records
// (original handle_bnet.cpp `game_spacer = {1,0,0,0}`).
inline constexpr std::uint32_t kGameListReplySpacer = 1u;

core::Result<GameListRequest> decode_game_list_req(const Packet& pkt) {
    Reader r{pkt.payload};
    GameListRequest m;
    auto gt = r.read_le<std::uint16_t>();
    if (!gt) return core::fail(gt.error());
    m.gametype = gt.value();
    auto u1 = r.read_le<std::uint16_t>();
    if (!u1) return core::fail(u1.error());
    m.unknown1 = u1.value();
    RD_U32(m.unknown2);
    RD_U32(m.unknown3);
    RD_U32(m.max_games);
    RD_STR(m.game_name);
    return m;
}

core::Result<GameListReply> decode_game_list_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    GameListReply m;
    std::uint32_t game_count = 0;
    RD_U32(game_count);
    RD_U32(m.sstatus);
    // Cap entries; legacy protocol uses small lists. Defensive bound.
    if (game_count > 1024u) {
        return core::fail(core::make_error(
            core::StatusCode::InvalidArgument,
            "bnet codec: GAMELISTREPLY game_count > 1024"));
    }
    m.entries.resize(game_count);
    bool first = true;
    for (auto& e : m.entries) {
        // A 4-byte spacer dword precedes every record except the first
        // (mirrors the original GAMELISTREPLY `game_spacer`).
        if (!first) {
            auto sp = r.read_le<std::uint32_t>();
            if (!sp) return core::fail(sp.error());
        }
        first = false;
        auto gt = r.read_le<std::uint16_t>();
        if (!gt) return core::fail(gt.error());
        e.gametype = gt.value();
        auto u1 = r.read_le<std::uint16_t>();
        if (!u1) return core::fail(u1.error());
        e.unknown1 = u1.value();
        auto u3 = r.read_le<std::uint16_t>();
        if (!u3) return core::fail(u3.error());
        e.unknown3 = u3.value();
        // port + game_ip are big-endian on the wire.
        auto pt = r.read_be<std::uint16_t>();
        if (!pt) return core::fail(pt.error());
        e.port = pt.value();
        auto ip = r.read_be<std::uint32_t>();
        if (!ip) return core::fail(ip.error());
        e.game_ip = ip.value();
        RD_U32(e.unknown4);
        RD_U32(e.unknown5);
        RD_U32(e.status);
        RD_U32(e.unknown6);
        RD_STR(e.game_name);
        RD_STR(e.password);
        RD_STR(e.info);
    }
    return m;
}

core::Result<StartGame4Request> decode_startgame4_request(const Packet& pkt) {
    Reader r{pkt.payload};
    StartGame4Request m;
    auto st = r.read_le<std::uint16_t>();
    if (!st) return core::fail(st.error());
    m.status = st.value();
    auto fl = r.read_le<std::uint16_t>();
    if (!fl) return core::fail(fl.error());
    m.flag = fl.value();
    RD_U32(m.unknown2);
    auto gt = r.read_le<std::uint16_t>();
    if (!gt) return core::fail(gt.error());
    m.gametype = gt.value();
    auto op = r.read_le<std::uint16_t>();
    if (!op) return core::fail(op.error());
    m.option = op.value();
    RD_U32(m.unknown4);
    RD_U32(m.unknown5);
    RD_STR(m.game_name);
    auto pw = r.read_cstring();
    if (!pw) return core::fail(pw.error());
    m.password = pw.value();
    auto info = r.read_cstring();
    if (!info) return core::fail(info.error());
    m.info = info.value();
    return m;
}

core::Result<StartGame4Ack> decode_startgame4_ack(const Packet& pkt) {
    Reader r{pkt.payload};
    StartGame4Ack m;
    RD_U32(m.reply);
    return m;
}

// --- SID_UDPPINGRESPONSE (0x14) -------------------------------------------

core::Result<UdpOk> decode_udp_ok(const Packet& pkt) {
    Reader r{pkt.payload};
    UdpOk m;
    RD_U32(m.echo);
    return m;
}

// --- SID_GETLADDERDATA (0x2E) ---------------------------------------------

core::Result<NetGamePort> decode_netgameport(const Packet& pkt) {
    Reader r{pkt.payload};
    NetGamePort m;
    RD_U16(m.port);
    return m;
}

// --- Game-lifecycle decoders --------------------------------------------

// 0x02 CLIENT_CLOSEGAME / 0x1F CLIENT_CLOSEGAME2 — empty body.

core::Result<CloseGame> decode_close_game(const Packet& pkt) {
    auto s = check_empty_body(pkt);
    if (!s) return core::fail(s.error());
    return CloseGame{};
}

core::Result<CloseGame2> decode_close_game2(const Packet& pkt) {
    auto s = check_empty_body(pkt);
    if (!s) return core::fail(s.error());
    return CloseGame2{};
}

// 0x08 CLIENT_STARTGAME1

core::Result<StartGame1Request> decode_startgame1_request(const Packet& pkt) {
    Reader r{pkt.payload};
    StartGame1Request m;
    RD_U32(m.status);
    RD_U32(m.unknown3);
    RD_U16(m.gametype);
    RD_U16(m.unknown1);
    RD_U32(m.unknown4);
    RD_U32(m.unknown5);
    RD_STR(m.game_name);
    RD_STR(m.password);
    RD_STR(m.info);
    return m;
}

core::Result<StartGame1Ack> decode_startgame1_ack(const Packet& pkt) {
    Reader r{pkt.payload};
    StartGame1Ack m;
    RD_U32(m.reply);
    return m;
}

// 0x1A CLIENT_STARTGAME3

core::Result<StartGame3Request> decode_startgame3_request(const Packet& pkt) {
    Reader r{pkt.payload};
    StartGame3Request m;
    RD_U32(m.status);
    RD_U32(m.unknown3);
    RD_U16(m.gametype);
    RD_U16(m.unknown1);
    RD_U32(m.unknown6);
    RD_U32(m.unknown4);
    RD_U32(m.unknown5);
    RD_STR(m.game_name);
    RD_STR(m.password);
    RD_STR(m.info);
    return m;
}

core::Result<StartGame3Ack> decode_startgame3_ack(const Packet& pkt) {
    Reader r{pkt.payload};
    StartGame3Ack m;
    RD_U32(m.reply);
    return m;
}

// 0x22 CLIENT_JOIN_GAME

core::Result<JoinGame> decode_join_game(const Packet& pkt) {
    Reader r{pkt.payload};
    JoinGame m;
    RD_U32(m.clienttag);
    RD_U32(m.versiontag);
    RD_STR(m.game_name);
    RD_STR(m.password);
    return m;
}

// 0x2C CLIENT_GAME_REPORT

core::Result<GameReport> decode_game_report(const Packet& pkt) {
    Reader r{pkt.payload};
    GameReport m;
    RD_U32(m.unknown1);
    std::uint32_t count = 0;
    RD_U32(count);
    // Bound count to remaining bytes to prevent OOM/DoS via crafted packet.
    // Each result is 4 bytes + at least 1 byte for cstring name; cap reasonably.
    if (count > r.remaining() / 4u) {
        return core::fail(core::Error{core::StatusCode::InvalidArgument,
            "bnet codec: GAMEREPORT count exceeds payload"});
    }
    m.results.resize(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        RD_U32(m.results[i]);
    }
    m.player_names.resize(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        auto s = r.read_cstring();
        if (!s) return core::fail(s.error());
        m.player_names[i] = s.value();
    }
    RD_STR(m.report_header);
    RD_STR(m.report_body);
    return m;
}

// --- Misc / anti-cheat / advisory decoders ------------------------------

// 0x17 SERVER_READMEMORY: request_id, address, length.

core::Result<ReadMemoryRequest> decode_read_memory_request(const Packet& pkt) {
    Reader r{pkt.payload};
    ReadMemoryRequest m;
    RD_U32(m.request_id);
    RD_U32(m.address);
    RD_U32(m.length);
    return m;
}

// 0x17 CLIENT_READMEMORY: request_id followed by raw memory bytes (rest of payload).

core::Result<ReadMemoryReply> decode_read_memory_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    ReadMemoryReply m;
    RD_U32(m.request_id);
    auto tail = r.tail();
    m.memory.resize(tail.size());
    for (std::size_t i = 0; i < tail.size(); ++i) {
        m.memory[i] = static_cast<std::uint8_t>(tail[i]);
    }
    return m;
}

// 0x1B CLIENT_UNKNOWN_1B: u16 unknown1, u16 port_be, u32 ip_be, u32, u32.

core::Result<Unknown1B> decode_unknown_1b(const Packet& pkt) {
    Reader r{pkt.payload};
    Unknown1B m;
    RD_U16(m.unknown1);
    RD_U16(m.port_be);
    RD_U32(m.ip_be);
    RD_U32(m.unknown2);
    RD_U32(m.unknown3);
    return m;
}

// 0x24 CLIENT_UNKNOWN_24: empty body.

core::Result<Unknown24> decode_unknown_24(const Packet& pkt) {
    auto s = check_empty_body(pkt);
    if (!s) return core::fail(s.error());
    return Unknown24{};
}

// 0x32 CLIENT_MAPAUTHREQ1: 5×u32 checksum, cstring mapfile.

core::Result<MapAuthReq1> decode_mapauthreq1(const Packet& pkt) {
    Reader r{pkt.payload};
    MapAuthReq1 m;
    RD_HASH5(m.file_checksum);
    RD_STR(m.mapfile);
    return m;
}

core::Result<MapAuthReply1> decode_mapauthreply1(const Packet& pkt) {
    Reader r{pkt.payload};
    MapAuthReply1 m;
    RD_U32(m.response);
    return m;
}

// 0x3C CLIENT_MAPAUTHREQ2: u32 unknown, 5×u32 hash, cstring mapfile.

core::Result<MapAuthReq2> decode_mapauthreq2(const Packet& pkt) {
    Reader r{pkt.payload};
    MapAuthReq2 m;
    RD_U32(m.unknown);
    RD_HASH5(m.file_hash);
    RD_STR(m.mapfile);
    return m;
}

core::Result<MapAuthReply2> decode_mapauthreply2(const Packet& pkt) {
    Reader r{pkt.payload};
    MapAuthReply2 m;
    RD_U32(m.response);
    return m;
}

// 0x5C CLIENT_CHANGECLIENT: u32 clienttag.

core::Result<ChangeClient> decode_change_client(const Packet& pkt) {
    Reader r{pkt.payload};
    ChangeClient m;
    RD_U32(m.clienttag);
    return m;
}

#undef RD_U64
#undef RD_U16


} // namespace detail

core::Status<> encode(Writer& w, const GameListRequest& m) {
    w.begin_bnet_packet(kSidGetAdvListEx);
    w.write_le<std::uint16_t>(m.gametype);
    w.write_le<std::uint16_t>(m.unknown1);
    w.write_le<std::uint32_t>(m.unknown2);
    w.write_le<std::uint32_t>(m.unknown3);
    w.write_le<std::uint32_t>(m.max_games);
    w.write_cstring(m.game_name);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const GameListReply& m) {
    if (m.entries.size() > 1024u) {
        return core::fail(core::make_error(
            core::StatusCode::InvalidArgument,
            "bnet codec: GAMELISTREPLY entries > 1024"));
    }
    w.begin_bnet_packet(kSidGetAdvListEx);
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(m.entries.size()));
    w.write_le<std::uint32_t>(m.sstatus);
    bool first = true;
    for (const auto& e : m.entries) {
        // The original emits a 4-byte spacer dword (value 1) before every game
        // record except the first (handle_bnet.cpp `game_spacer = {1,0,0,0}`).
        if (!first) {
            w.write_le<std::uint32_t>(detail::kGameListReplySpacer);
        }
        first = false;
        w.write_le<std::uint16_t>(e.gametype);
        w.write_le<std::uint16_t>(e.unknown1);
        w.write_le<std::uint16_t>(e.unknown3);
        w.write_be<std::uint16_t>(e.port);
        w.write_be<std::uint32_t>(e.game_ip);
        w.write_le<std::uint32_t>(e.unknown4);
        w.write_le<std::uint32_t>(e.unknown5);
        w.write_le<std::uint32_t>(e.status);
        w.write_le<std::uint32_t>(e.unknown6);
        w.write_cstring(e.game_name);
        w.write_cstring(e.password);
        w.write_cstring(e.info);
    }
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const StartGame4Request& m) {
    w.begin_bnet_packet(kSidStartGame4);
    w.write_le<std::uint16_t>(m.status);
    w.write_le<std::uint16_t>(m.flag);
    w.write_le<std::uint32_t>(m.unknown2);
    w.write_le<std::uint16_t>(m.gametype);
    w.write_le<std::uint16_t>(m.option);
    w.write_le<std::uint32_t>(m.unknown4);
    w.write_le<std::uint32_t>(m.unknown5);
    w.write_cstring(m.game_name);
    w.write_cstring(m.password);
    w.write_cstring(m.info);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const StartGame4Ack& m) {
    w.begin_bnet_packet(kSidStartGame4);
    w.write_le<std::uint32_t>(m.reply);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const UdpOk& m) {
    w.begin_bnet_packet(kSidUdpOk);
    w.write_le<std::uint32_t>(m.echo);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const NetGamePort& m) {
    w.begin_bnet_packet(kSidNetGamePort);
    w.write_le<std::uint16_t>(m.port);
    return w.finalize_bnet_packet();
}

// --- Game-lifecycle encoders --------------------------------------------

core::Status<> encode(Writer& w, const CloseGame&) {
    w.begin_bnet_packet(kSidCloseGame);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const CloseGame2&) {
    w.begin_bnet_packet(kSidCloseGame2);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const StartGame1Request& m) {
    w.begin_bnet_packet(kSidStartGame1);
    w.write_le<std::uint32_t>(m.status);
    w.write_le<std::uint32_t>(m.unknown3);
    w.write_le<std::uint16_t>(m.gametype);
    w.write_le<std::uint16_t>(m.unknown1);
    w.write_le<std::uint32_t>(m.unknown4);
    w.write_le<std::uint32_t>(m.unknown5);
    w.write_cstring(m.game_name);
    w.write_cstring(m.password);
    w.write_cstring(m.info);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const StartGame1Ack& m) {
    w.begin_bnet_packet(kSidStartGame1);
    w.write_le<std::uint32_t>(m.reply);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const StartGame3Request& m) {
    w.begin_bnet_packet(kSidStartGame3);
    w.write_le<std::uint32_t>(m.status);
    w.write_le<std::uint32_t>(m.unknown3);
    w.write_le<std::uint16_t>(m.gametype);
    w.write_le<std::uint16_t>(m.unknown1);
    w.write_le<std::uint32_t>(m.unknown6);
    w.write_le<std::uint32_t>(m.unknown4);
    w.write_le<std::uint32_t>(m.unknown5);
    w.write_cstring(m.game_name);
    w.write_cstring(m.password);
    w.write_cstring(m.info);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const StartGame3Ack& m) {
    w.begin_bnet_packet(kSidStartGame3);
    w.write_le<std::uint32_t>(m.reply);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const JoinGame& m) {
    w.begin_bnet_packet(kSidJoinGame);
    w.write_le<std::uint32_t>(m.clienttag);
    w.write_le<std::uint32_t>(m.versiontag);
    w.write_cstring(m.game_name);
    w.write_cstring(m.password);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const GameReport& m) {
    if (m.results.size() != m.player_names.size()) {
        return core::fail(core::Error{core::StatusCode::InvalidArgument,
            "bnet codec: GAMEREPORT results/player_names size mismatch"});
    }
    w.begin_bnet_packet(kSidGameReport);
    w.write_le<std::uint32_t>(m.unknown1);
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(m.results.size()));
    for (auto r : m.results) w.write_le<std::uint32_t>(r);
    for (const auto& n : m.player_names) w.write_cstring(n);
    w.write_cstring(m.report_header);
    w.write_cstring(m.report_body);
    return w.finalize_bnet_packet();
}

// --- Misc / anti-cheat / advisory encoders ------------------------------

core::Status<> encode(Writer& w, const ReadMemoryRequest& m) {
    w.begin_bnet_packet(kSidReadMemory);
    w.write_le<std::uint32_t>(m.request_id);
    w.write_le<std::uint32_t>(m.address);
    w.write_le<std::uint32_t>(m.length);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ReadMemoryReply& m) {
    w.begin_bnet_packet(kSidReadMemory);
    w.write_le<std::uint32_t>(m.request_id);
    for (auto b : m.memory) w.write_le<std::uint8_t>(b);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const Unknown1B& m) {
    w.begin_bnet_packet(kSidUnknown1B);
    w.write_le<std::uint16_t>(m.unknown1);
    w.write_le<std::uint16_t>(m.port_be);
    w.write_le<std::uint32_t>(m.ip_be);
    w.write_le<std::uint32_t>(m.unknown2);
    w.write_le<std::uint32_t>(m.unknown3);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const Unknown24&) {
    w.begin_bnet_packet(kSidUnknown24);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const MapAuthReq1& m) {
    w.begin_bnet_packet(kSidMapAuth1);
    for (auto v : m.file_checksum) w.write_le<std::uint32_t>(v);
    w.write_cstring(m.mapfile);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const MapAuthReply1& m) {
    w.begin_bnet_packet(kSidMapAuth1);
    w.write_le<std::uint32_t>(m.response);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const MapAuthReq2& m) {
    w.begin_bnet_packet(kSidMapAuth2);
    w.write_le<std::uint32_t>(m.unknown);
    for (auto v : m.file_hash) w.write_le<std::uint32_t>(v);
    w.write_cstring(m.mapfile);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const MapAuthReply2& m) {
    w.begin_bnet_packet(kSidMapAuth2);
    w.write_le<std::uint32_t>(m.response);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ChangeClient& m) {
    w.begin_bnet_packet(kSidChangeClient);
    w.write_le<std::uint32_t>(m.clienttag);
    return w.finalize_bnet_packet();
}


} // namespace pvpgn::protocol::bnet
