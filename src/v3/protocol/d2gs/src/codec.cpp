// SPDX-License-Identifier: GPL-2.0-or-later
#include "protocol/d2gs/codec.hpp"

#include <algorithm>

#include "core/error.hpp"
#include "protocol/common/reader.hpp"

namespace pvpgn::protocol::d2gs {

namespace {

core::Status<> emit(Writer& w, std::uint16_t type, std::uint32_t seqno,
                    Writer& payload) {
    const auto body = payload.view();
    const auto total = body.size() + D2gsHeader::kSize;
    if (total > 0xFFFFu) {
        return core::fail(core::Error{
            core::StatusCode::OutOfRange, "d2gs codec: too large"});
    }
    w.write_le<std::uint16_t>(static_cast<std::uint16_t>(total));
    w.write_le<std::uint16_t>(type);
    w.write_le<std::uint32_t>(seqno);
    w.write_bytes(body);
    return core::ok();
}

}  // namespace

core::Result<D2gsHeader> parse_header(core::ByteView buf) {
    Reader r{buf};
    auto sz = r.read_le<std::uint16_t>();
    if (!sz) return core::fail(sz.error());
    auto tp = r.read_le<std::uint16_t>();
    if (!tp) return core::fail(tp.error());
    auto sq = r.read_le<std::uint32_t>();
    if (!sq) return core::fail(sq.error());
    if (sz.value() < D2gsHeader::kSize) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument, "d2gs codec: size < header"});
    }
    return D2gsHeader{sz.value(), tp.value(), sq.value()};
}

namespace {

core::Result<SetGsInfo> decode_set_gs_info(core::ByteView body,
                                           std::uint32_t seqno) {
    Reader r{body};
    SetGsInfo m{seqno};
    auto a = r.read_le<std::uint32_t>(); if (!a) return core::fail(a.error());
    auto b = r.read_le<std::uint32_t>(); if (!b) return core::fail(b.error());
    m.max_game = a.value();
    m.gameflag = b.value();
    return m;
}

core::Result<Control> decode_control(core::ByteView body,
                                     std::uint32_t seqno) {
    Reader r{body};
    Control m{seqno};
    auto a = r.read_le<std::uint32_t>(); if (!a) return core::fail(a.error());
    auto b = r.read_le<std::uint32_t>(); if (!b) return core::fail(b.error());
    m.cmd   = a.value();
    m.value = b.value();
    return m;
}

}  // namespace

core::Result<DownMessage> decode_d2cs_to_d2gs(core::ByteView buf) {
    auto hdr = parse_header(buf);
    if (!hdr) return core::fail(hdr.error());
    if (buf.size() < hdr.value().size) {
        return core::fail(core::Error{
            core::StatusCode::OutOfRange, "d2gs codec: incomplete"});
    }
    auto body = buf.subspan(D2gsHeader::kSize,
                            static_cast<std::size_t>(hdr.value().size) -
                                D2gsHeader::kSize);
    switch (hdr.value().type) {
        case kAuthReq: {
            Reader r{body};
            DownAuthReq m{};
            m.seqno = hdr.value().seqno;
            auto sn = r.read_le<std::uint32_t>(); if (!sn) return core::fail(sn.error());
            auto sl = r.read_le<std::uint32_t>(); if (!sl) return core::fail(sl.error());
            auto rn = r.read_cstring();           if (!rn) return core::fail(rn.error());
            auto cs = r.read_bytes(r.remaining());
            if (!cs) return core::fail(cs.error());
            m.session_num = sn.value();
            (void)sl;  // signlen captured implicitly by checksum length
            m.realm_name.assign(rn.value());
            m.key_checksum.assign(cs.value().begin(), cs.value().end());
            return DownMessage{std::move(m)};
        }
        case kAuthReply: {
            Reader r{body};
            auto v = r.read_le<std::uint32_t>();
            if (!v) return core::fail(v.error());
            return DownMessage{DownAuthReply{hdr.value().seqno, v.value()}};
        }
        case kSetGsInfo: {
            auto m = decode_set_gs_info(body, hdr.value().seqno);
            if (!m) return core::fail(m.error());
            return DownMessage{m.value()};
        }
        case kEcho:
            return DownMessage{EchoReq{hdr.value().seqno}};
        case kControl: {
            auto m = decode_control(body, hdr.value().seqno);
            if (!m) return core::fail(m.error());
            return DownMessage{m.value()};
        }
        default:
            return core::fail(core::Error{
                core::StatusCode::Unimplemented, "d2gs codec: unknown type"});
    }
}

core::Result<UpMessage> decode_d2gs_to_d2cs(core::ByteView buf) {
    auto hdr = parse_header(buf);
    if (!hdr) return core::fail(hdr.error());
    if (buf.size() < hdr.value().size) {
        return core::fail(core::Error{
            core::StatusCode::OutOfRange, "d2gs codec: incomplete"});
    }
    auto body = buf.subspan(D2gsHeader::kSize,
                            static_cast<std::size_t>(hdr.value().size) -
                                D2gsHeader::kSize);
    switch (hdr.value().type) {
        case kAuthReply: {
            Reader r{body};
            UpAuthReply m{};
            m.seqno = hdr.value().seqno;
            auto v = r.read_le<std::uint32_t>(); if (!v) return core::fail(v.error()); m.version  = v.value();
            v      = r.read_le<std::uint32_t>(); if (!v) return core::fail(v.error()); m.checksum = v.value();
            v      = r.read_le<std::uint32_t>(); if (!v) return core::fail(v.error()); m.randnum  = v.value();
            v      = r.read_le<std::uint32_t>(); if (!v) return core::fail(v.error()); m.signlen  = v.value();
            auto sg = r.read_bytes(128);
            if (!sg) return core::fail(sg.error());
            std::copy(sg.value().begin(), sg.value().end(), m.sign.begin());
            return UpMessage{m};
        }
        case kSetGsInfo: {
            auto m = decode_set_gs_info(body, hdr.value().seqno);
            if (!m) return core::fail(m.error());
            return UpMessage{m.value()};
        }
        case kEcho:
            return UpMessage{EchoReply{hdr.value().seqno}};
        default:
            return core::fail(core::Error{
                core::StatusCode::Unimplemented, "d2gs codec: unknown type"});
    }
}

core::Status<> encode(Writer& w, const SetGsInfo& m) {
    Writer p;
    p.write_le<std::uint32_t>(m.max_game);
    p.write_le<std::uint32_t>(m.gameflag);
    return emit(w, kSetGsInfo, m.seqno, p);
}

core::Status<> encode(Writer& w, const EchoReq& m) {
    Writer p;
    return emit(w, kEcho, m.seqno, p);
}

core::Status<> encode(Writer& w, const EchoReply& m) {
    Writer p;
    return emit(w, kEcho, m.seqno, p);
}

core::Status<> encode(Writer& w, const Control& m) {
    Writer p;
    p.write_le<std::uint32_t>(m.cmd);
    p.write_le<std::uint32_t>(m.value);
    return emit(w, kControl, m.seqno, p);
}

core::Status<> encode(Writer& w, const DownAuthReq& m) {
    Writer p;
    p.write_le<std::uint32_t>(m.session_num);
    p.write_le<std::uint32_t>(static_cast<std::uint32_t>(m.key_checksum.size()));
    p.write_cstring(m.realm_name);
    p.write_bytes(core::ByteView{m.key_checksum.data(),
                                 m.key_checksum.size()});
    return emit(w, kAuthReq, m.seqno, p);
}

core::Status<> encode(Writer& w, const DownAuthReply& m) {
    Writer p;
    p.write_le<std::uint32_t>(m.reply);
    return emit(w, kAuthReply, m.seqno, p);
}

core::Status<> encode(Writer& w, const UpAuthReply& m) {
    Writer p;
    p.write_le<std::uint32_t>(m.version);
    p.write_le<std::uint32_t>(m.checksum);
    p.write_le<std::uint32_t>(m.randnum);
    p.write_le<std::uint32_t>(m.signlen);
    p.write_bytes(core::ByteView{m.sign.data(), m.sign.size()});
    return emit(w, kAuthReply, m.seqno, p);
}

}  // namespace pvpgn::protocol::d2gs
