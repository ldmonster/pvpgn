// SPDX-License-Identifier: GPL-2.0-or-later
#include "protocol/d2cs/codec.hpp"

#include "core/error.hpp"
#include "protocol/common/reader.hpp"

namespace pvpgn::protocol::d2cs {

namespace {

core::Status<> emit(Writer& w, std::uint8_t type, Writer& payload) {
    const auto body = payload.view();
    const auto total = body.size() + D2csHeader::kSize;
    if (total > 0xFFFFu) {
        return core::fail(core::Error{
            core::StatusCode::OutOfRange, "d2cs codec: too large"});
    }
    w.write_le<std::uint16_t>(static_cast<std::uint16_t>(total));
    w.write_le<std::uint8_t>(type);
    w.write_bytes(body);
    return core::ok();
}

core::Result<core::ByteView> body_of(core::ByteView buf, D2csHeader& out) {
    auto hdr = parse_header(buf);
    if (!hdr) return core::fail(hdr.error());
    if (buf.size() < hdr.value().size) {
        return core::fail(core::Error{
            core::StatusCode::OutOfRange, "d2cs codec: incomplete"});
    }
    out = hdr.value();
    return buf.subspan(D2csHeader::kSize,
                       static_cast<std::size_t>(hdr.value().size) -
                           D2csHeader::kSize);
}

}  // namespace

core::Result<D2csHeader> parse_header(core::ByteView buf) {
    Reader r{buf};
    auto sz = r.read_le<std::uint16_t>();
    if (!sz) return core::fail(sz.error());
    auto tp = r.read_le<std::uint8_t>();
    if (!tp) return core::fail(tp.error());
    if (sz.value() < D2csHeader::kSize) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument, "d2cs codec: size < header"});
    }
    return D2csHeader{sz.value(), tp.value()};
}

core::Result<ClientMessage> decode_client(core::ByteView buf) {
    D2csHeader hdr{};
    auto body = body_of(buf, hdr);
    if (!body) return core::fail(body.error());
    Reader r{body.value()};

    switch (hdr.type) {
        case kClientLoginReq: {
            LoginReq m;
            auto u = r.read_le<std::uint32_t>(); if (!u) return core::fail(u.error()); m.seqno       = u.value();
            u      = r.read_le<std::uint32_t>(); if (!u) return core::fail(u.error()); m.u1          = u.value();
            u      = r.read_le<std::uint32_t>(); if (!u) return core::fail(u.error()); m.bncs_addr1  = u.value();
            u      = r.read_le<std::uint32_t>(); if (!u) return core::fail(u.error()); m.session_num = u.value();
            u      = r.read_le<std::uint32_t>(); if (!u) return core::fail(u.error()); m.session_key = u.value();
            u      = r.read_le<std::uint32_t>(); if (!u) return core::fail(u.error()); m.cdkey_id    = u.value();
            u      = r.read_le<std::uint32_t>(); if (!u) return core::fail(u.error()); m.u5          = u.value();
            u      = r.read_le<std::uint32_t>(); if (!u) return core::fail(u.error()); m.client_tag  = u.value();
            u      = r.read_le<std::uint32_t>(); if (!u) return core::fail(u.error()); m.bn_version  = u.value();
            u      = r.read_le<std::uint32_t>(); if (!u) return core::fail(u.error()); m.bncs_addr2  = u.value();
            u      = r.read_le<std::uint32_t>(); if (!u) return core::fail(u.error()); m.u6          = u.value();
            for (auto& word : m.secret_hash) {
                auto v = r.read_le<std::uint32_t>();
                if (!v) return core::fail(v.error());
                word = v.value();
            }
            auto s = r.read_cstring();
            if (!s) return core::fail(s.error());
            m.account_name.assign(s.value());
            return ClientMessage{std::move(m)};
        }
        case kClientCreateCharReq: {
            CreateCharReq m;
            auto a = r.read_le<std::uint16_t>(); if (!a) return core::fail(a.error()); m.chclass = a.value();
            a      = r.read_le<std::uint16_t>(); if (!a) return core::fail(a.error()); m.u1      = a.value();
            a      = r.read_le<std::uint16_t>(); if (!a) return core::fail(a.error()); m.status  = a.value();
            auto s = r.read_cstring();
            if (!s) return core::fail(s.error());
            m.name.assign(s.value());
            return ClientMessage{std::move(m)};
        }
        case kClientCreateGameReq: {
            CreateGameReq m;
            auto sq = r.read_le<std::uint16_t>(); if (!sq) return core::fail(sq.error()); m.seqno     = sq.value();
            auto gf = r.read_le<std::uint32_t>(); if (!gf) return core::fail(gf.error()); m.gameflag  = gf.value();
            auto u  = r.read_le<std::uint8_t>();  if (!u)  return core::fail(u.error());  m.u1        = u.value();
            u       = r.read_le<std::uint8_t>();  if (!u)  return core::fail(u.error());  m.leveldiff = u.value();
            u       = r.read_le<std::uint8_t>();  if (!u)  return core::fail(u.error());  m.maxchar   = u.value();
            auto s1 = r.read_cstring(); if (!s1) return core::fail(s1.error()); m.game_name.assign(s1.value());
            auto s2 = r.read_cstring(); if (!s2) return core::fail(s2.error()); m.game_pass.assign(s2.value());
            auto s3 = r.read_cstring(); if (!s3) return core::fail(s3.error()); m.game_desc.assign(s3.value());
            return ClientMessage{std::move(m)};
        }
        case kClientJoinGameReq: {
            JoinGameReq m;
            auto sq = r.read_le<std::uint16_t>(); if (!sq) return core::fail(sq.error()); m.seqno = sq.value();
            auto s1 = r.read_cstring(); if (!s1) return core::fail(s1.error()); m.game_name.assign(s1.value());
            auto s2 = r.read_cstring(); if (!s2) return core::fail(s2.error()); m.game_pass.assign(s2.value());
            return ClientMessage{std::move(m)};
        }
        case kClientGameListReq: {
            GameListReq m;
            auto sq = r.read_le<std::uint16_t>(); if (!sq) return core::fail(sq.error()); m.seqno    = sq.value();
            auto gf = r.read_le<std::uint32_t>(); if (!gf) return core::fail(gf.error()); m.gameflag = gf.value();
            return ClientMessage{m};
        }
        case kClientGameInfoReq: {
            GameInfoReq m;
            auto sq = r.read_le<std::uint16_t>(); if (!sq) return core::fail(sq.error()); m.seqno = sq.value();
            auto s  = r.read_cstring(); if (!s) return core::fail(s.error()); m.game_name.assign(s.value());
            return ClientMessage{std::move(m)};
        }
        case kClientCharListReq: {
            CharListReq m;
            auto a = r.read_le<std::uint16_t>(); if (!a) return core::fail(a.error()); m.maxchar = a.value();
            a      = r.read_le<std::uint16_t>(); if (!a) return core::fail(a.error()); m.u1      = a.value();
            return ClientMessage{m};
        }
        default:
            return core::fail(core::Error{
                core::StatusCode::Unimplemented, "d2cs codec: unknown type"});
    }
}

core::Result<ServerMessage> decode_server(core::ByteView buf) {
    D2csHeader hdr{};
    auto body = body_of(buf, hdr);
    if (!body) return core::fail(body.error());
    Reader r{body.value()};
    switch (hdr.type) {
        case kClientLoginReply: {
            auto v = r.read_le<std::uint32_t>();
            if (!v) return core::fail(v.error());
            return ServerMessage{LoginReply{v.value()}};
        }
        case kClientCreateCharReply: {
            auto v = r.read_le<std::uint32_t>();
            if (!v) return core::fail(v.error());
            return ServerMessage{CreateCharReply{v.value()}};
        }
        case kClientCreateGameReply: {
            CreateGameReply m;
            auto a = r.read_le<std::uint16_t>(); if (!a) return core::fail(a.error()); m.seqno  = a.value();
            a      = r.read_le<std::uint16_t>(); if (!a) return core::fail(a.error()); m.gameid = a.value();
            a      = r.read_le<std::uint16_t>(); if (!a) return core::fail(a.error()); m.u1     = a.value();
            auto v = r.read_le<std::uint32_t>(); if (!v) return core::fail(v.error()); m.reply  = v.value();
            return ServerMessage{m};
        }
        case kClientJoinGameReply: {
            JoinGameReply m;
            auto a = r.read_le<std::uint16_t>(); if (!a) return core::fail(a.error()); m.seqno  = a.value();
            a      = r.read_le<std::uint16_t>(); if (!a) return core::fail(a.error()); m.gameid = a.value();
            a      = r.read_le<std::uint16_t>(); if (!a) return core::fail(a.error()); m.u1     = a.value();
            auto v = r.read_le<std::uint32_t>(); if (!v) return core::fail(v.error()); m.addr   = v.value();
            v      = r.read_le<std::uint32_t>(); if (!v) return core::fail(v.error()); m.token  = v.value();
            v      = r.read_le<std::uint32_t>(); if (!v) return core::fail(v.error()); m.reply  = v.value();
            return ServerMessage{m};
        }
        case kClientGameListReply: {
            GameListReply m;
            auto sq = r.read_le<std::uint16_t>(); if (!sq) return core::fail(sq.error()); m.seqno    = sq.value();
            auto tk = r.read_le<std::uint32_t>(); if (!tk) return core::fail(tk.error()); m.token    = tk.value();
            auto cc = r.read_le<std::uint8_t>();  if (!cc) return core::fail(cc.error()); m.currchar = cc.value();
            auto gf = r.read_le<std::uint32_t>(); if (!gf) return core::fail(gf.error()); m.gameflag = gf.value();
            auto s1 = r.read_cstring(); if (!s1) return core::fail(s1.error()); m.game_name.assign(s1.value());
            auto s2 = r.read_cstring(); if (!s2) return core::fail(s2.error()); m.game_desc.assign(s2.value());
            return ServerMessage{std::move(m)};
        }
        case kClientGameInfoReply: {
            GameInfoReply m;
            auto sq = r.read_le<std::uint16_t>(); if (!sq) return core::fail(sq.error()); m.seqno     = sq.value();
            auto gf = r.read_le<std::uint32_t>(); if (!gf) return core::fail(gf.error()); m.gameflag  = gf.value();
            auto et = r.read_le<std::uint32_t>(); if (!et) return core::fail(et.error()); m.etime     = et.value();
            auto u  = r.read_le<std::uint8_t>();  if (!u)  return core::fail(u.error());  m.charlevel = u.value();
            u       = r.read_le<std::uint8_t>();  if (!u)  return core::fail(u.error());  m.leveldiff = u.value();
            u       = r.read_le<std::uint8_t>();  if (!u)  return core::fail(u.error());  m.maxchar   = u.value();
            u       = r.read_le<std::uint8_t>();  if (!u)  return core::fail(u.error());  m.currchar  = u.value();
            for (auto& b : m.chclass) {
                auto v = r.read_le<std::uint8_t>();
                if (!v) return core::fail(v.error());
                b = v.value();
            }
            for (auto& b : m.charlevels) {
                auto v = r.read_le<std::uint8_t>();
                if (!v) return core::fail(v.error());
                b = v.value();
            }
            auto s = r.read_cstring();
            if (!s) return core::fail(s.error());
            m.game_desc.assign(s.value());
            // currchar character names follow.
            if (m.currchar > 16) {
                return core::fail(core::Error{
                    core::StatusCode::InvalidArgument,
                    "d2cs codec: gameinfo currchar > 16"});
            }
            m.char_names.reserve(m.currchar);
            for (std::uint8_t i = 0; i < m.currchar; ++i) {
                auto nm = r.read_cstring();
                if (!nm) return core::fail(nm.error());
                m.char_names.emplace_back(nm.value());
            }
            return ServerMessage{std::move(m)};
        }
        case kClientCharListReply: {
            CharListReply m;
            auto a = r.read_le<std::uint16_t>(); if (!a) return core::fail(a.error()); m.maxchar   = a.value();
            a      = r.read_le<std::uint16_t>(); if (!a) return core::fail(a.error()); m.currchar  = a.value();
            a      = r.read_le<std::uint16_t>(); if (!a) return core::fail(a.error()); m.u1        = a.value();
            a      = r.read_le<std::uint16_t>(); if (!a) return core::fail(a.error()); m.currchar2 = a.value();
            // Defensive: legacy clients cap at 8 chars/account.
            if (m.currchar > 64) {
                return core::fail(core::Error{
                    core::StatusCode::InvalidArgument,
                    "d2cs codec: charlist currchar > 64"});
            }
            m.chars.reserve(m.currchar);
            for (std::uint16_t i = 0; i < m.currchar; ++i) {
                CharListEntry e;
                auto nm = r.read_cstring();
                if (!nm) return core::fail(nm.error());
                e.name.assign(nm.value());
                for (auto& b : e.portrait) {
                    auto v = r.read_le<std::uint8_t>();
                    if (!v) return core::fail(v.error());
                    b = v.value();
                }
                m.chars.push_back(std::move(e));
            }
            return ServerMessage{std::move(m)};
        }
        default:
            return core::fail(core::Error{
                core::StatusCode::Unimplemented, "d2cs codec: unknown type"});
    }
}

core::Status<> encode(Writer& w, const LoginReq& m) {
    Writer p;
    p.write_le<std::uint32_t>(m.seqno);
    p.write_le<std::uint32_t>(m.u1);
    p.write_le<std::uint32_t>(m.bncs_addr1);
    p.write_le<std::uint32_t>(m.session_num);
    p.write_le<std::uint32_t>(m.session_key);
    p.write_le<std::uint32_t>(m.cdkey_id);
    p.write_le<std::uint32_t>(m.u5);
    p.write_le<std::uint32_t>(m.client_tag);
    p.write_le<std::uint32_t>(m.bn_version);
    p.write_le<std::uint32_t>(m.bncs_addr2);
    p.write_le<std::uint32_t>(m.u6);
    for (auto word : m.secret_hash) p.write_le<std::uint32_t>(word);
    p.write_cstring(m.account_name);
    return emit(w, kClientLoginReq, p);
}

core::Status<> encode(Writer& w, const LoginReply& m) {
    Writer p;
    p.write_le<std::uint32_t>(m.reply);
    return emit(w, kClientLoginReply, p);
}

core::Status<> encode(Writer& w, const CreateCharReq& m) {
    Writer p;
    p.write_le<std::uint16_t>(m.chclass);
    p.write_le<std::uint16_t>(m.u1);
    p.write_le<std::uint16_t>(m.status);
    p.write_cstring(m.name);
    return emit(w, kClientCreateCharReq, p);
}

core::Status<> encode(Writer& w, const CreateCharReply& m) {
    Writer p;
    p.write_le<std::uint32_t>(m.reply);
    return emit(w, kClientCreateCharReply, p);
}

core::Status<> encode(Writer& w, const CharLoginReply& m) {
    Writer p;
    p.write_le<std::uint32_t>(m.reply);
    return emit(w, kClientCharLoginReply, p);
}

core::Status<> encode(Writer& w, const CreateGameReq& m) {
    Writer p;
    p.write_le<std::uint16_t>(m.seqno);
    p.write_le<std::uint32_t>(m.gameflag);
    p.write_le<std::uint8_t>(m.u1);
    p.write_le<std::uint8_t>(m.leveldiff);
    p.write_le<std::uint8_t>(m.maxchar);
    p.write_cstring(m.game_name);
    p.write_cstring(m.game_pass);
    p.write_cstring(m.game_desc);
    return emit(w, kClientCreateGameReq, p);
}

core::Status<> encode(Writer& w, const CreateGameReply& m) {
    Writer p;
    p.write_le<std::uint16_t>(m.seqno);
    p.write_le<std::uint16_t>(m.gameid);
    p.write_le<std::uint16_t>(m.u1);
    p.write_le<std::uint32_t>(m.reply);
    return emit(w, kClientCreateGameReply, p);
}

core::Status<> encode(Writer& w, const JoinGameReq& m) {
    Writer p;
    p.write_le<std::uint16_t>(m.seqno);
    p.write_cstring(m.game_name);
    p.write_cstring(m.game_pass);
    return emit(w, kClientJoinGameReq, p);
}

core::Status<> encode(Writer& w, const JoinGameReply& m) {
    Writer p;
    p.write_le<std::uint16_t>(m.seqno);
    p.write_le<std::uint16_t>(m.gameid);
    p.write_le<std::uint16_t>(m.u1);
    p.write_le<std::uint32_t>(m.addr);
    p.write_le<std::uint32_t>(m.token);
    p.write_le<std::uint32_t>(m.reply);
    return emit(w, kClientJoinGameReply, p);
}

core::Status<> encode(Writer& w, const GameListReq& m) {
    Writer p;
    p.write_le<std::uint16_t>(m.seqno);
    p.write_le<std::uint32_t>(m.gameflag);
    return emit(w, kClientGameListReq, p);
}

core::Status<> encode(Writer& w, const GameListReply& m) {
    Writer p;
    p.write_le<std::uint16_t>(m.seqno);
    p.write_le<std::uint32_t>(m.token);
    p.write_le<std::uint8_t>(m.currchar);
    p.write_le<std::uint32_t>(m.gameflag);
    p.write_cstring(m.game_name);
    p.write_cstring(m.game_desc);
    return emit(w, kClientGameListReply, p);
}

core::Status<> encode(Writer& w, const GameInfoReq& m) {
    Writer p;
    p.write_le<std::uint16_t>(m.seqno);
    p.write_cstring(m.game_name);
    return emit(w, kClientGameInfoReq, p);
}

core::Status<> encode(Writer& w, const GameInfoReply& m) {
    Writer p;
    p.write_le<std::uint16_t>(m.seqno);
    p.write_le<std::uint32_t>(m.gameflag);
    p.write_le<std::uint32_t>(m.etime);
    p.write_le<std::uint8_t>(m.charlevel);
    p.write_le<std::uint8_t>(m.leveldiff);
    p.write_le<std::uint8_t>(m.maxchar);
    p.write_le<std::uint8_t>(m.currchar);
    for (auto b : m.chclass)    p.write_le<std::uint8_t>(b);
    for (auto b : m.charlevels) p.write_le<std::uint8_t>(b);
    p.write_cstring(m.game_desc);
    for (const auto& nm : m.char_names) p.write_cstring(nm);
    return emit(w, kClientGameInfoReply, p);
}

core::Status<> encode(Writer& w, const CharListReq& m) {
    Writer p;
    p.write_le<std::uint16_t>(m.maxchar);
    p.write_le<std::uint16_t>(m.u1);
    return emit(w, kClientCharListReq, p);
}

core::Status<> encode(Writer& w, const CharListReply& m) {
    Writer p;
    p.write_le<std::uint16_t>(m.maxchar);
    p.write_le<std::uint16_t>(m.currchar);
    p.write_le<std::uint16_t>(m.u1);
    p.write_le<std::uint16_t>(m.currchar2);
    for (const auto& e : m.chars) {
        p.write_cstring(e.name);
        for (auto b : e.portrait) p.write_le<std::uint8_t>(b);
    }
    return emit(w, kClientCharListReply, p);
}

}  // namespace pvpgn::protocol::d2cs
