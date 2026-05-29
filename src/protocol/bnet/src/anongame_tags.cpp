// SPDX-License-Identifier: GPL-2.0-or-later
//
// Per-tag typed payload parsers/serializers for FINDANONGAME INFOREPLY.
// See `anongame_tags.hpp` for layout notes. These operate on *decompressed*
// bytes only; compression is the caller's responsibility.

#include "protocol/bnet/anongame_tags.hpp"

#include <utility>

#include "core/bytes.hpp"
#include "core/error.hpp"
#include "protocol/common/reader.hpp"
#include "protocol/common/writer.hpp"

namespace pvpgn::protocol::bnet {

namespace {

core::ByteView view_of(const std::vector<std::uint8_t>& bytes) noexcept {
    return core::ByteView{reinterpret_cast<const std::byte*>(bytes.data()),
                          bytes.size()};
}

std::vector<std::uint8_t> writer_to_u8(Writer&& w) {
    auto buf = std::move(w).take();
    std::vector<std::uint8_t> out;
    out.resize(buf.size());
    for (std::size_t i = 0; i < buf.size(); ++i) {
        out[i] = static_cast<std::uint8_t>(buf[i]);
    }
    return out;
}

core::Status<> require_eof(Reader& r, const char* ctx) {
    if (!r.empty()) {
        return core::fail(core::make_error(
            core::StatusCode::OutOfRange,
            std::string{"anongame tag payload: trailing bytes after "} + ctx));
    }
    return core::ok();
}

}  // namespace

// ============================================================ URL ========

core::Result<AnonGameUrlPayload> parse_url_payload(
    const std::vector<std::uint8_t>& bytes, std::uint8_t expected_count) {
    Reader r{view_of(bytes)};
    AnonGameUrlPayload out;
    while (!r.empty()) {
        auto s = r.read_cstring();
        if (!s) return core::fail(s.error());
        out.urls.emplace_back(s.value());
    }
    if (expected_count != 0 && out.urls.size() != expected_count) {
        return core::fail(core::make_error(
            core::StatusCode::OutOfRange,
            "URL payload: string count does not match expected"));
    }
    return out;
}

std::vector<std::uint8_t> serialize_url_payload(const AnonGameUrlPayload& m) {
    Writer w;
    for (const auto& s : m.urls) w.write_cstring(s);
    return writer_to_u8(std::move(w));
}

// ============================================================ MAP ========

core::Result<AnonGameMapPayload> parse_map_payload(
    const std::vector<std::uint8_t>& bytes) {
    Reader r{view_of(bytes)};
    AnonGameMapPayload out;
    auto cnt = r.read_le<std::uint8_t>();
    if (!cnt) return core::fail(cnt.error());
    out.mapnames.reserve(cnt.value());
    for (std::uint8_t i = 0; i < cnt.value(); ++i) {
        auto s = r.read_cstring();
        if (!s) return core::fail(s.error());
        out.mapnames.emplace_back(s.value());
    }
    if (auto s = require_eof(r, "MAP entries"); !s) return core::fail(s.error());
    return out;
}

std::vector<std::uint8_t> serialize_map_payload(const AnonGameMapPayload& m) {
    Writer w;
    // count is u8; truncating is the caller's responsibility but we clamp
    // defensively to match the wire shape.
    const std::uint8_t cnt = m.mapnames.size() > 0xFFu
        ? std::uint8_t{0xFFu}
        : static_cast<std::uint8_t>(m.mapnames.size());
    w.write_le<std::uint8_t>(cnt);
    for (std::size_t i = 0; i < cnt; ++i) w.write_cstring(m.mapnames[i]);
    return writer_to_u8(std::move(w));
}

// ============================================================ TYPE =======

core::Result<AnonGameTypePayload> parse_type_payload(
    const std::vector<std::uint8_t>& bytes) {
    Reader r{view_of(bytes)};
    AnonGameTypePayload out;
    auto sc = r.read_le<std::uint8_t>();
    if (!sc) return core::fail(sc.error());
    out.sections.reserve(sc.value());
    for (std::uint8_t i = 0; i < sc.value(); ++i) {
        AnonGameTypeSection sec;
        auto sid = r.read_le<std::uint8_t>();
        if (!sid) return core::fail(sid.error());
        sec.section_id = sid.value();
        auto gc = r.read_le<std::uint8_t>();
        if (!gc) return core::fail(gc.error());
        sec.gamestyles.reserve(gc.value());
        for (std::uint8_t g = 0; g < gc.value(); ++g) {
            AnonGameTypeGamestyle gs;
            auto px = r.read_bytes(gs.prefix.size());
            if (!px) return core::fail(px.error());
            for (std::size_t b = 0; b < gs.prefix.size(); ++b) {
                gs.prefix[b] = static_cast<std::uint8_t>(px.value()[b]);
            }
            auto mc = r.read_le<std::uint8_t>();
            if (!mc) return core::fail(mc.error());
            auto mi = r.read_bytes(mc.value());
            if (!mi) return core::fail(mi.error());
            gs.map_indices.resize(mc.value());
            for (std::size_t b = 0; b < mc.value(); ++b) {
                gs.map_indices[b] = static_cast<std::uint8_t>(mi.value()[b]);
            }
            sec.gamestyles.push_back(std::move(gs));
        }
        out.sections.push_back(std::move(sec));
    }
    if (auto s = require_eof(r, "TYPE sections"); !s) return core::fail(s.error());
    return out;
}

std::vector<std::uint8_t> serialize_type_payload(const AnonGameTypePayload& m) {
    Writer w;
    w.write_le<std::uint8_t>(static_cast<std::uint8_t>(m.sections.size()));
    for (const auto& sec : m.sections) {
        w.write_le<std::uint8_t>(sec.section_id);
        w.write_le<std::uint8_t>(static_cast<std::uint8_t>(sec.gamestyles.size()));
        for (const auto& gs : sec.gamestyles) {
            for (auto b : gs.prefix) w.write_le<std::uint8_t>(b);
            w.write_le<std::uint8_t>(static_cast<std::uint8_t>(gs.map_indices.size()));
            for (auto b : gs.map_indices) w.write_le<std::uint8_t>(b);
        }
    }
    return writer_to_u8(std::move(w));
}

// ============================================================ DESC =======

core::Result<AnonGameDescPayload> parse_desc_payload(
    const std::vector<std::uint8_t>& bytes) {
    Reader r{view_of(bytes)};
    AnonGameDescPayload out;
    auto cnt = r.read_le<std::uint8_t>();
    if (!cnt) return core::fail(cnt.error());
    out.entries.reserve(cnt.value());
    for (std::uint8_t i = 0; i < cnt.value(); ++i) {
        AnonGameDescEntry e;
        auto sid = r.read_le<std::uint8_t>(); if (!sid) return core::fail(sid.error());
        e.section_id = sid.value();
        auto gid = r.read_le<std::uint8_t>(); if (!gid) return core::fail(gid.error());
        e.gametype_id = gid.value();
        auto sd = r.read_cstring(); if (!sd) return core::fail(sd.error());
        e.short_desc = std::string{sd.value()};
        auto ld = r.read_cstring(); if (!ld) return core::fail(ld.error());
        e.long_desc = std::string{ld.value()};
        out.entries.push_back(std::move(e));
    }
    if (auto s = require_eof(r, "DESC entries"); !s) return core::fail(s.error());
    return out;
}

std::vector<std::uint8_t> serialize_desc_payload(const AnonGameDescPayload& m) {
    Writer w;
    w.write_le<std::uint8_t>(static_cast<std::uint8_t>(m.entries.size()));
    for (const auto& e : m.entries) {
        w.write_le<std::uint8_t>(e.section_id);
        w.write_le<std::uint8_t>(e.gametype_id);
        w.write_cstring(e.short_desc);
        w.write_cstring(e.long_desc);
    }
    return writer_to_u8(std::move(w));
}

// ============================================================ LADR =======

core::Result<AnonGameLadrPayload> parse_ladr_payload(
    const std::vector<std::uint8_t>& bytes) {
    Reader r{view_of(bytes)};
    AnonGameLadrPayload out;
    auto cnt = r.read_le<std::uint8_t>();
    if (!cnt) return core::fail(cnt.error());
    out.entries.reserve(cnt.value());
    for (std::uint8_t i = 0; i < cnt.value(); ++i) {
        AnonGameLadrEntry e;
        auto tag = r.read_le<std::uint32_t>(); if (!tag) return core::fail(tag.error());
        e.tag = tag.value();
        auto d = r.read_cstring(); if (!d) return core::fail(d.error());
        e.desc = std::string{d.value()};
        auto u = r.read_cstring(); if (!u) return core::fail(u.error());
        e.url = std::string{u.value()};
        out.entries.push_back(std::move(e));
    }
    if (auto s = require_eof(r, "LADR entries"); !s) return core::fail(s.error());
    return out;
}

std::vector<std::uint8_t> serialize_ladr_payload(const AnonGameLadrPayload& m) {
    Writer w;
    w.write_le<std::uint8_t>(static_cast<std::uint8_t>(m.entries.size()));
    for (const auto& e : m.entries) {
        w.write_le<std::uint32_t>(e.tag);
        w.write_cstring(e.desc);
        w.write_cstring(e.url);
    }
    return writer_to_u8(std::move(w));
}

}  // namespace pvpgn::protocol::bnet
