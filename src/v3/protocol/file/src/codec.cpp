// SPDX-License-Identifier: GPL-2.0-or-later
#include "protocol/file/codec.hpp"

#include <variant>

#include "core/error.hpp"
#include "protocol/common/reader.hpp"

namespace pvpgn::protocol::file {

core::Result<FileHeader> parse_header(core::ByteView buf) {
    Reader r{buf};
    auto sz = r.read_le<std::uint16_t>();
    if (!sz) return core::fail(sz.error());
    auto tp = r.read_le<std::uint16_t>();
    if (!tp) return core::fail(tp.error());
    if (sz.value() < FileHeader::kSize) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "file codec: size < header"});
    }
    return FileHeader{sz.value(), tp.value()};
}

core::Result<Message> decode(core::ByteView buf) {
    auto hdr = parse_header(buf);
    if (!hdr) return core::fail(hdr.error());
    if (buf.size() < hdr.value().size) {
        return core::fail(core::Error{
            core::StatusCode::OutOfRange, "file codec: incomplete packet"});
    }
    auto body = buf.subspan(FileHeader::kSize,
                            static_cast<std::size_t>(hdr.value().size) -
                                FileHeader::kSize);
    Reader r{body};
    switch (hdr.value().type) {
        case kClientFileReq: {
            ClientFileReq m;
            auto a = r.read_le<std::uint32_t>(); if (!a) return core::fail(a.error()); m.arch_tag      = a.value();
            a      = r.read_le<std::uint32_t>(); if (!a) return core::fail(a.error()); m.client_tag    = a.value();
            a      = r.read_le<std::uint32_t>(); if (!a) return core::fail(a.error()); m.ad_id         = a.value();
            a      = r.read_le<std::uint32_t>(); if (!a) return core::fail(a.error()); m.extension_tag = a.value();
            a      = r.read_le<std::uint32_t>(); if (!a) return core::fail(a.error()); m.start_offset  = a.value();
            auto t = r.read_le<std::uint64_t>(); if (!t) return core::fail(t.error()); m.timestamp     = t.value();
            auto s = r.read_cstring();           if (!s) return core::fail(s.error()); m.filename.assign(s.value());
            return Message{std::move(m)};
        }
        case kServerFileReply: {
            ServerFileReply m;
            auto a = r.read_le<std::uint32_t>(); if (!a) return core::fail(a.error()); m.file_len      = a.value();
            a      = r.read_le<std::uint32_t>(); if (!a) return core::fail(a.error()); m.ad_id         = a.value();
            a      = r.read_le<std::uint32_t>(); if (!a) return core::fail(a.error()); m.extension_tag = a.value();
            auto t = r.read_le<std::uint64_t>(); if (!t) return core::fail(t.error()); m.timestamp     = t.value();
            auto s = r.read_cstring();           if (!s) return core::fail(s.error()); m.filename.assign(s.value());
            return Message{std::move(m)};
        }
        default:
            return core::fail(core::Error{
                core::StatusCode::Unimplemented,
                "file codec: unknown type"});
    }
}

namespace {

// Encode helper: build payload into a temporary, then prepend the
// size+type header so we don't need a poke API.
core::Status<> emit(Writer& w, std::uint16_t type, Writer& payload) {
    const auto body = payload.view();
    const auto total = body.size() + FileHeader::kSize;
    if (total > 0xFFFFu) {
        return core::fail(core::Error{
            core::StatusCode::OutOfRange, "file codec: too large"});
    }
    w.write_le<std::uint16_t>(static_cast<std::uint16_t>(total));
    w.write_le<std::uint16_t>(type);
    w.write_bytes(body);
    return core::ok();
}

}  // namespace

core::Status<> encode(Writer& w, const ClientFileReq& m) {
    Writer p;
    p.write_le<std::uint32_t>(m.arch_tag);
    p.write_le<std::uint32_t>(m.client_tag);
    p.write_le<std::uint32_t>(m.ad_id);
    p.write_le<std::uint32_t>(m.extension_tag);
    p.write_le<std::uint32_t>(m.start_offset);
    p.write_le<std::uint64_t>(m.timestamp);
    p.write_cstring(m.filename);
    return emit(w, kClientFileReq, p);
}

core::Status<> encode(Writer& w, const ServerFileReply& m) {
    Writer p;
    p.write_le<std::uint32_t>(m.file_len);
    p.write_le<std::uint32_t>(m.ad_id);
    p.write_le<std::uint32_t>(m.extension_tag);
    p.write_le<std::uint64_t>(m.timestamp);
    p.write_cstring(m.filename);
    return emit(w, kServerFileReply, p);
}

}  // namespace pvpgn::protocol::file
