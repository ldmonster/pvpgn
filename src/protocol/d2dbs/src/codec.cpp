// SPDX-License-Identifier: GPL-2.0-or-later
#include "protocol/d2dbs/codec.hpp"

#include <cstring>
#include <string>

#include "core/error.hpp"
#include "protocol/common/reader.hpp"

namespace pvpgn::protocol::d2dbs {

namespace {

core::Status<> emit_empty(Writer& w, std::uint16_t type,
                          std::uint32_t seqno) {
    w.write_le<std::uint16_t>(D2dbsHeader::kSize);
    w.write_le<std::uint16_t>(type);
    w.write_le<std::uint32_t>(seqno);
    return core::ok();
}

constexpr std::uint16_t framed_size(std::size_t body_size) {
    return static_cast<std::uint16_t>(D2dbsHeader::kSize + body_size);
}

void write_header(Writer& w, std::uint16_t total_size,
                  std::uint16_t type, std::uint32_t seqno) {
    w.write_le<std::uint16_t>(total_size);
    w.write_le<std::uint16_t>(type);
    w.write_le<std::uint32_t>(seqno);
}

void write_cstr(Writer& w, const std::string& s) {
    for (char c : s) w.write_le<std::uint8_t>(static_cast<std::uint8_t>(c));
    w.write_le<std::uint8_t>(0x00);
}

void write_blob(Writer& w, const std::vector<std::uint8_t>& v) {
    for (auto b : v) w.write_le<std::uint8_t>(b);
}

core::Result<std::string> read_cstr(Reader& r) {
    auto sv = r.read_cstring();
    if (!sv) return core::fail(sv.error());
    return std::string{sv.value()};
}

core::Result<SaveDataRequest> dec_save_data_req(const D2dbsHeader& hdr,
                                                core::ByteView buf) {
    Reader r{buf.subspan(D2dbsHeader::kSize, hdr.size - D2dbsHeader::kSize)};
    SaveDataRequest m;
    m.seqno = hdr.seqno;
    auto dt = r.read_le<std::uint16_t>(); if (!dt) return core::fail(dt.error());
    m.datatype = dt.value();
    auto dl = r.read_le<std::uint16_t>(); if (!dl) return core::fail(dl.error());
    const std::uint16_t datalen = dl.value();
    auto an = read_cstr(r); if (!an) return core::fail(an.error());
    m.account = std::move(an.value());
    auto cn = read_cstr(r); if (!cn) return core::fail(cn.error());
    m.charname = std::move(cn.value());
    // RealmName precedes the data blob on the wire (original dbs_packet_savedata
    // reads AccountName, CharName, RealmName, then the datalen-byte blob).
    auto rn = read_cstr(r); if (!rn) return core::fail(rn.error());
    m.realm = std::move(rn.value());
    auto blob = r.read_bytes(datalen); if (!blob) return core::fail(blob.error());
    m.data.resize(datalen);
    for (std::size_t i = 0; i < datalen; ++i) {
        m.data[i] = static_cast<std::uint8_t>(blob.value()[i]);
    }
    return m;
}

core::Result<SaveDataReply> dec_save_data_reply(const D2dbsHeader& hdr,
                                                core::ByteView buf) {
    Reader r{buf.subspan(D2dbsHeader::kSize, hdr.size - D2dbsHeader::kSize)};
    SaveDataReply m;
    m.seqno = hdr.seqno;
    auto rs = r.read_le<std::uint32_t>(); if (!rs) return core::fail(rs.error());
    m.result = rs.value();
    auto dt = r.read_le<std::uint16_t>(); if (!dt) return core::fail(dt.error());
    m.datatype = dt.value();
    auto cn = read_cstr(r); if (!cn) return core::fail(cn.error());
    m.charname = std::move(cn.value());
    return m;
}

core::Result<GetDataRequest> dec_get_data_req(const D2dbsHeader& hdr,
                                              core::ByteView buf) {
    Reader r{buf.subspan(D2dbsHeader::kSize, hdr.size - D2dbsHeader::kSize)};
    GetDataRequest m;
    m.seqno = hdr.seqno;
    auto dt = r.read_le<std::uint16_t>(); if (!dt) return core::fail(dt.error());
    m.datatype = dt.value();
    auto an = read_cstr(r); if (!an) return core::fail(an.error());
    m.account = std::move(an.value());
    auto cn = read_cstr(r); if (!cn) return core::fail(cn.error());
    m.charname = std::move(cn.value());
    // RealmName trails AccountName + CharName (original dbs_packet_getdata).
    auto rn = read_cstr(r); if (!rn) return core::fail(rn.error());
    m.realm = std::move(rn.value());
    return m;
}

core::Result<GetDataReply> dec_get_data_reply(const D2dbsHeader& hdr,
                                              core::ByteView buf) {
    Reader r{buf.subspan(D2dbsHeader::kSize, hdr.size - D2dbsHeader::kSize)};
    GetDataReply m;
    m.seqno = hdr.seqno;
    auto rs = r.read_le<std::uint32_t>(); if (!rs) return core::fail(rs.error());
    m.result = rs.value();
    auto cct = r.read_le<std::uint32_t>(); if (!cct) return core::fail(cct.error());
    m.charcreatetime = cct.value();
    auto al = r.read_le<std::uint32_t>(); if (!al) return core::fail(al.error());
    m.allowladder = al.value();
    auto dt = r.read_le<std::uint16_t>(); if (!dt) return core::fail(dt.error());
    m.datatype = dt.value();
    auto dl = r.read_le<std::uint16_t>(); if (!dl) return core::fail(dl.error());
    const std::uint16_t datalen = dl.value();
    auto cn = read_cstr(r); if (!cn) return core::fail(cn.error());
    m.charname = std::move(cn.value());
    auto blob = r.read_bytes(datalen); if (!blob) return core::fail(blob.error());
    m.data.resize(datalen);
    for (std::size_t i = 0; i < datalen; ++i) {
        m.data[i] = static_cast<std::uint8_t>(blob.value()[i]);
    }
    return m;
}

core::Result<UpdateLadderRequest> dec_update_ladder(const D2dbsHeader& hdr,
                                                    core::ByteView buf) {
    Reader r{buf.subspan(D2dbsHeader::kSize, hdr.size - D2dbsHeader::kSize)};
    UpdateLadderRequest m;
    m.seqno = hdr.seqno;
    auto cl = r.read_le<std::uint32_t>(); if (!cl) return core::fail(cl.error());
    m.charlevel = cl.value();
    auto el = r.read_le<std::uint32_t>(); if (!el) return core::fail(el.error());
    m.charexplow = el.value();
    auto eh = r.read_le<std::uint32_t>(); if (!eh) return core::fail(eh.error());
    m.charexphigh = eh.value();
    auto cc = r.read_le<std::uint16_t>(); if (!cc) return core::fail(cc.error());
    m.charclass = cc.value();
    auto cs = r.read_le<std::uint16_t>(); if (!cs) return core::fail(cs.error());
    m.charstatus = cs.value();
    auto cn = read_cstr(r); if (!cn) return core::fail(cn.error());
    m.charname = std::move(cn.value());
    auto rn = read_cstr(r); if (!rn) return core::fail(rn.error());
    m.realmname = std::move(rn.value());
    return m;
}

core::Result<CharLockRequest> dec_char_lock(const D2dbsHeader& hdr,
                                            core::ByteView buf) {
    Reader r{buf.subspan(D2dbsHeader::kSize, hdr.size - D2dbsHeader::kSize)};
    CharLockRequest m;
    m.seqno = hdr.seqno;
    auto ls = r.read_le<std::uint32_t>(); if (!ls) return core::fail(ls.error());
    m.lockstatus = ls.value();
    auto cn = read_cstr(r); if (!cn) return core::fail(cn.error());
    m.charname = std::move(cn.value());
    auto rn = read_cstr(r); if (!rn) return core::fail(rn.error());
    m.realmname = std::move(rn.value());
    return m;
}

}  // namespace

core::Result<D2dbsHeader> parse_header(core::ByteView buf) {
    Reader r{buf};
    auto sz = r.read_le<std::uint16_t>();
    if (!sz) return core::fail(sz.error());
    auto tp = r.read_le<std::uint16_t>();
    if (!tp) return core::fail(tp.error());
    auto sq = r.read_le<std::uint32_t>();
    if (!sq) return core::fail(sq.error());
    if (sz.value() < D2dbsHeader::kSize) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "d2dbs codec: size < header"});
    }
    return D2dbsHeader{sz.value(), tp.value(), sq.value()};
}

core::Result<DownMessage> decode_d2dbs_to_d2gs(core::ByteView buf) {
    auto hdr = parse_header(buf);
    if (!hdr) return core::fail(hdr.error());
    if (buf.size() < hdr.value().size) {
        return core::fail(core::Error{
            core::StatusCode::OutOfRange, "d2dbs codec: incomplete"});
    }
    switch (hdr.value().type) {
        case kEcho:
            return DownMessage{EchoRequest{hdr.value().seqno}};
        case kSaveData: {
            auto m = dec_save_data_reply(hdr.value(), buf);
            if (!m) return core::fail(m.error());
            return DownMessage{std::move(m.value())};
        }
        case kGetData: {
            auto m = dec_get_data_reply(hdr.value(), buf);
            if (!m) return core::fail(m.error());
            return DownMessage{std::move(m.value())};
        }
        default:
            return core::fail(core::Error{
                core::StatusCode::Unimplemented,
                "d2dbs codec: unknown downstream type"});
    }
}

core::Result<UpMessage> decode_d2gs_to_d2dbs(core::ByteView buf) {
    auto hdr = parse_header(buf);
    if (!hdr) return core::fail(hdr.error());
    if (buf.size() < hdr.value().size) {
        return core::fail(core::Error{
            core::StatusCode::OutOfRange, "d2dbs codec: incomplete"});
    }
    switch (hdr.value().type) {
        case kEcho:
            return UpMessage{EchoReply{hdr.value().seqno}};
        case kSaveData: {
            auto m = dec_save_data_req(hdr.value(), buf);
            if (!m) return core::fail(m.error());
            return UpMessage{std::move(m.value())};
        }
        case kGetData: {
            auto m = dec_get_data_req(hdr.value(), buf);
            if (!m) return core::fail(m.error());
            return UpMessage{std::move(m.value())};
        }
        case kUpdateLadder: {
            auto m = dec_update_ladder(hdr.value(), buf);
            if (!m) return core::fail(m.error());
            return UpMessage{std::move(m.value())};
        }
        case kCharLock: {
            auto m = dec_char_lock(hdr.value(), buf);
            if (!m) return core::fail(m.error());
            return UpMessage{std::move(m.value())};
        }
        default:
            return core::fail(core::Error{
                core::StatusCode::Unimplemented,
                "d2dbs codec: unknown upstream type"});
    }
}

core::Result<ConnectHandshake> decode_connect_handshake(core::ByteView buf) {
    if (buf.empty()) {
        return core::fail(core::Error{
            core::StatusCode::OutOfRange,
            "d2dbs connect: empty buffer"});
    }
    auto cls = static_cast<std::uint8_t>(buf[0]);
    if (cls != kConnectClassD2gsToD2dbs) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "d2dbs connect: unknown class"});
    }
    return ConnectHandshake{cls};
}

core::Status<> encode(Writer& w, const ConnectHandshake& m) {
    w.write_le<std::uint8_t>(m.cclass);
    return core::ok();
}

core::Status<> encode(Writer& w, const EchoRequest& m) {
    return emit_empty(w, kEcho, m.seqno);
}

core::Status<> encode(Writer& w, const EchoReply& m) {
    return emit_empty(w, kEcho, m.seqno);
}

core::Status<> encode(Writer& w, const SaveDataRequest& m) {
    const std::uint16_t total = framed_size(
        std::size_t{2 + 2}
        + m.account.size() + 1
        + m.charname.size() + 1
        + m.realm.size() + 1
        + m.data.size());
    write_header(w, total, kSaveData, m.seqno);
    w.write_le<std::uint16_t>(m.datatype);
    w.write_le<std::uint16_t>(static_cast<std::uint16_t>(m.data.size()));
    write_cstr(w, m.account);
    write_cstr(w, m.charname);
    write_cstr(w, m.realm);
    write_blob(w, m.data);
    return core::ok();
}

core::Status<> encode(Writer& w, const SaveDataReply& m) {
    const std::uint16_t total = framed_size(
        std::size_t{4 + 2} + m.charname.size() + 1);
    write_header(w, total, kSaveData, m.seqno);
    w.write_le<std::uint32_t>(m.result);
    w.write_le<std::uint16_t>(m.datatype);
    write_cstr(w, m.charname);
    return core::ok();
}

core::Status<> encode(Writer& w, const GetDataRequest& m) {
    const std::uint16_t total = framed_size(
        std::size_t{2}
        + m.account.size() + 1
        + m.charname.size() + 1
        + m.realm.size() + 1);
    write_header(w, total, kGetData, m.seqno);
    w.write_le<std::uint16_t>(m.datatype);
    write_cstr(w, m.account);
    write_cstr(w, m.charname);
    write_cstr(w, m.realm);
    return core::ok();
}

core::Status<> encode(Writer& w, const GetDataReply& m) {
    const std::uint16_t total = framed_size(
        std::size_t{4 + 4 + 4 + 2 + 2}
        + m.charname.size() + 1
        + m.data.size());
    write_header(w, total, kGetData, m.seqno);
    w.write_le<std::uint32_t>(m.result);
    w.write_le<std::uint32_t>(m.charcreatetime);
    w.write_le<std::uint32_t>(m.allowladder);
    w.write_le<std::uint16_t>(m.datatype);
    w.write_le<std::uint16_t>(static_cast<std::uint16_t>(m.data.size()));
    write_cstr(w, m.charname);
    write_blob(w, m.data);
    return core::ok();
}

core::Status<> encode(Writer& w, const UpdateLadderRequest& m) {
    const std::uint16_t total = framed_size(
        std::size_t{4 + 4 + 4 + 2 + 2}
        + m.charname.size() + 1
        + m.realmname.size() + 1);
    write_header(w, total, kUpdateLadder, m.seqno);
    w.write_le<std::uint32_t>(m.charlevel);
    w.write_le<std::uint32_t>(m.charexplow);
    w.write_le<std::uint32_t>(m.charexphigh);
    w.write_le<std::uint16_t>(m.charclass);
    w.write_le<std::uint16_t>(m.charstatus);
    write_cstr(w, m.charname);
    write_cstr(w, m.realmname);
    return core::ok();
}

core::Status<> encode(Writer& w, const CharLockRequest& m) {
    const std::uint16_t total = framed_size(
        std::size_t{4}
        + m.charname.size() + 1
        + m.realmname.size() + 1);
    write_header(w, total, kCharLock, m.seqno);
    w.write_le<std::uint32_t>(m.lockstatus);
    write_cstr(w, m.charname);
    write_cstr(w, m.realmname);
    return core::ok();
}

}  // namespace pvpgn::protocol::d2dbs
