// SPDX-License-Identifier: GPL-2.0-or-later
//
// Typed sub-message parser/serializer for SID_WARCRAFTGENERAL (0x44).
// Operates over the opaque `data` field of WarcraftGeneralRequest /
// WarcraftGeneralReply that the wire codec produces.

#include "protocol/bnet/anongame.hpp"

#include <utility>

#include "core/bytes.hpp"
#include "core/error.hpp"
#include "protocol/common/reader.hpp"
#include "protocol/common/writer.hpp"

namespace pvpgn::protocol::bnet {

namespace {

core::ByteView view_of(const std::vector<std::byte>& v) noexcept {
    return core::ByteView{v.data(), v.size()};
}

std::vector<std::byte> take_bytes(Writer&& w) { return std::move(w).take(); }

core::Error unimplemented_sub(std::uint8_t opt, const char* side) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "FINDANONGAME %s sub-option 0x%02X", side,
                  static_cast<unsigned>(opt));
    return core::make_error(core::StatusCode::Unimplemented, buf);
}

// ----- client decoders --------------------------------------------------

core::Result<AnonGameSearch> dec_search(Reader& r) {
    AnonGameSearch m;
    {
        auto v = r.read_le<std::uint32_t>(); if (!v) return core::fail(v.error());
        m.count = v.value();
    }
    {
        auto v = r.read_le<std::uint32_t>(); if (!v) return core::fail(v.error());
        m.unknown2 = v.value();
    }
    {
        auto v = r.read_le<std::uint8_t>(); if (!v) return core::fail(v.error());
        m.type = v.value();
    }
    {
        auto v = r.read_le<std::uint8_t>(); if (!v) return core::fail(v.error());
        m.gametype = v.value();
    }
    {
        auto v = r.read_le<std::uint32_t>(); if (!v) return core::fail(v.error());
        m.map_prefs = v.value();
    }
    {
        auto v = r.read_le<std::uint8_t>(); if (!v) return core::fail(v.error());
        m.unknown3 = v.value();
    }
    {
        auto v = r.read_le<std::uint32_t>(); if (!v) return core::fail(v.error());
        m.id = v.value();
    }
    {
        auto v = r.read_le<std::uint32_t>(); if (!v) return core::fail(v.error());
        m.race = v.value();
    }
    return m;
}

template <class T>
core::Status<> read_info_array(Reader& r, std::array<T, 5>& info) {
    for (auto& w : info) {
        auto v = r.read_le<T>();
        if (!v) return core::fail(v.error());
        w = v.value();
    }
    return core::ok();
}

core::Result<AnonGameAtSearch> dec_at_search(Reader& r) {
    AnonGameAtSearch m;
    auto u32 = [&](std::uint32_t& dst) -> core::Status<> {
        auto v = r.read_le<std::uint32_t>(); if (!v) return core::fail(v.error());
        dst = v.value(); return core::ok();
    };
    auto u8 = [&](std::uint8_t& dst) -> core::Status<> {
        auto v = r.read_le<std::uint8_t>(); if (!v) return core::fail(v.error());
        dst = v.value(); return core::ok();
    };
    if (auto s = u32(m.count); !s) return core::fail(s.error());
    if (auto s = u32(m.tid); !s) return core::fail(s.error());
    if (auto s = u32(m.timestamp); !s) return core::fail(s.error());
    if (auto s = u8 (m.teamsize); !s) return core::fail(s.error());
    if (auto s = read_info_array<std::uint32_t>(r, m.info); !s) return core::fail(s.error());
    if (auto s = u32(m.unknown2); !s) return core::fail(s.error());
    if (auto s = u8 (m.unknown3); !s) return core::fail(s.error());
    if (auto s = u32(m.id); !s) return core::fail(s.error());
    if (auto s = u32(m.race); !s) return core::fail(s.error());
    return m;
}

core::Result<AnonGameAtInviterSearch> dec_at_inv(Reader& r) {
    AnonGameAtInviterSearch m;
    auto u32 = [&](std::uint32_t& dst) -> core::Status<> {
        auto v = r.read_le<std::uint32_t>(); if (!v) return core::fail(v.error());
        dst = v.value(); return core::ok();
    };
    auto u8 = [&](std::uint8_t& dst) -> core::Status<> {
        auto v = r.read_le<std::uint8_t>(); if (!v) return core::fail(v.error());
        dst = v.value(); return core::ok();
    };
    if (auto s = u32(m.count); !s) return core::fail(s.error());
    if (auto s = u32(m.tid); !s) return core::fail(s.error());
    if (auto s = u32(m.timestamp); !s) return core::fail(s.error());
    if (auto s = u8 (m.teamsize); !s) return core::fail(s.error());
    if (auto s = read_info_array<std::uint32_t>(r, m.info); !s) return core::fail(s.error());
    if (auto s = u32(m.unknown2); !s) return core::fail(s.error());
    if (auto s = u8 (m.type); !s) return core::fail(s.error());
    if (auto s = u8 (m.gametype); !s) return core::fail(s.error());
    if (auto s = u32(m.map_prefs); !s) return core::fail(s.error());
    if (auto s = u8 (m.unknown3); !s) return core::fail(s.error());
    if (auto s = u32(m.id); !s) return core::fail(s.error());
    if (auto s = u32(m.race); !s) return core::fail(s.error());
    return m;
}

core::Result<AnonGameInfoRequest> dec_inforeq(Reader& r) {
    AnonGameInfoRequest m;
    {
        auto v = r.read_le<std::uint32_t>(); if (!v) return core::fail(v.error());
        m.count = v.value();
    }
    {
        auto v = r.read_le<std::uint8_t>(); if (!v) return core::fail(v.error());
        m.noitems = v.value();
    }
    m.entries.reserve(m.noitems);
    for (std::uint8_t i = 0; i < m.noitems; ++i) {
        AnonGameInfoRequestEntry e;
        auto t = r.read_le<std::uint32_t>(); if (!t) return core::fail(t.error());
        e.tag = t.value();
        auto u = r.read_le<std::uint32_t>(); if (!u) return core::fail(u.error());
        e.tag_unk = u.value();
        m.entries.push_back(e);
    }
    return m;
}

core::Result<AnonGameClientCancel> dec_client_cancel(Reader& r) {
    AnonGameClientCancel m;
    auto v = r.read_le<std::uint32_t>(); if (!v) return core::fail(v.error());
    m.count = v.value();
    return m;
}

core::Result<AnonGameProfileRequest> dec_profile_req(Reader& r) {
    AnonGameProfileRequest m;
    auto c = r.read_le<std::uint32_t>(); if (!c) return core::fail(c.error());
    m.count = c.value();
    auto u = r.read_cstring(); if (!u) return core::fail(u.error());
    m.username = std::string{u.value()};
    auto t = r.read_cstring(); if (!t) return core::fail(t.error());
    m.client_tag = std::string{t.value()};
    return m;
}

core::Result<AnonGameTournamentRequest> dec_tournament_req(Reader& r) {
    AnonGameTournamentRequest m;
    auto v = r.read_le<std::uint32_t>(); if (!v) return core::fail(v.error());
    m.count = v.value();
    return m;
}

core::Result<AnonGameClanProfileRequest> dec_clan_profile(Reader& r) {
    AnonGameClanProfileRequest m;
    auto c = r.read_le<std::uint32_t>(); if (!c) return core::fail(c.error());
    m.count = c.value();
    auto t = r.read_le<std::uint32_t>(); if (!t) return core::fail(t.error());
    m.clan_tag = t.value();
    return m;
}

core::Result<AnonGameGetIcon> dec_get_icon(Reader& r) {
    AnonGameGetIcon m;
    auto v = r.read_le<std::uint32_t>(); if (!v) return core::fail(v.error());
    m.count = v.value();
    return m;
}

core::Result<AnonGameSetIcon> dec_set_icon(Reader& r) {
    AnonGameSetIcon m;
    auto c = r.read_le<std::uint32_t>(); if (!c) return core::fail(c.error());
    m.count = c.value();
    auto i = r.read_le<std::uint32_t>(); if (!i) return core::fail(i.error());
    m.icon = i.value();
    return m;
}

// ----- server decoders --------------------------------------------------

core::Result<AnonGameSearchReply> dec_search_reply(Reader& r) {
    AnonGameSearchReply m;
    auto c = r.read_le<std::uint32_t>(); if (!c) return core::fail(c.error());
    m.count = c.value();
    auto rep = r.read_le<std::uint32_t>(); if (!rep) return core::fail(rep.error());
    m.reply = rep.value();
    return m;
}

core::Result<AnonGameFound> dec_found(Reader& r) {
    AnonGameFound m;
    auto u32 = [&](std::uint32_t& dst) -> core::Status<> {
        auto v = r.read_le<std::uint32_t>(); if (!v) return core::fail(v.error());
        dst = v.value(); return core::ok();
    };
    auto u16 = [&](std::uint16_t& dst) -> core::Status<> {
        auto v = r.read_le<std::uint16_t>(); if (!v) return core::fail(v.error());
        dst = v.value(); return core::ok();
    };
    auto u8 = [&](std::uint8_t& dst) -> core::Status<> {
        auto v = r.read_le<std::uint8_t>(); if (!v) return core::fail(v.error());
        dst = v.value(); return core::ok();
    };
    if (auto s = u32(m.count); !s) return core::fail(s.error());
    if (auto s = u32(m.unknown1); !s) return core::fail(s.error());
    if (auto s = u32(m.ip_be); !s) return core::fail(s.error());
    if (auto s = u16(m.port_be); !s) return core::fail(s.error());
    if (auto s = u8 (m.unknown2); !s) return core::fail(s.error());
    if (auto s = u8 (m.unknown3); !s) return core::fail(s.error());
    if (auto s = u16(m.unknown4); !s) return core::fail(s.error());
    if (auto s = u32(m.id); !s) return core::fail(s.error());
    if (auto s = u8 (m.unknown5); !s) return core::fail(s.error());
    if (auto s = u8 (m.type); !s) return core::fail(s.error());
    if (auto s = u8 (m.gametype); !s) return core::fail(s.error());
    auto mp = r.read_cstring(); if (!mp) return core::fail(mp.error());
    m.mapname = std::string{mp.value()};
    if (auto s = u32(m.saf.unknown1); !s) return core::fail(s.error());
    if (auto s = u32(m.saf.anongame_string); !s) return core::fail(s.error());
    if (auto s = u8 (m.saf.totalplayers); !s) return core::fail(s.error());
    if (auto s = u8 (m.saf.totalteams); !s) return core::fail(s.error());
    if (auto s = u16(m.saf.unknown2); !s) return core::fail(s.error());
    if (auto s = u8 (m.saf.visibility); !s) return core::fail(s.error());
    if (auto s = u8 (m.saf.unknown3); !s) return core::fail(s.error());
    auto tail = r.tail();
    m.extras.resize(tail.size());
    for (std::size_t i = 0; i < tail.size(); ++i) {
        m.extras[i] = static_cast<std::uint8_t>(tail[i]);
    }
    return m;
}

core::Result<AnonGameServerCancel> dec_server_cancel(Reader& r) {
    AnonGameServerCancel m;
    auto v = r.read_le<std::uint32_t>(); if (!v) return core::fail(v.error());
    m.count = v.value();
    return m;
}

core::Result<AnonGameInfoReply> dec_inforeply(Reader& r) {
    AnonGameInfoReply m;
    auto c = r.read_le<std::uint32_t>(); if (!c) return core::fail(c.error());
    m.count = c.value();
    auto n = r.read_le<std::uint8_t>(); if (!n) return core::fail(n.error());
    m.noitems = n.value();
    // After the typed prefix the rest is: tag(4) + tag_unk(4) + payload(N) + trailing(1).
    // If fewer than 9 bytes remain we treat the whole envelope as malformed.
    if (r.remaining() < 9) {
        return core::fail(core::make_error(
            core::StatusCode::OutOfRange,
            "FINDANONGAME INFOREPLY: short tag/payload/trailing"));
    }
    auto t = r.read_le<std::uint32_t>(); if (!t) return core::fail(t.error());
    m.tag = t.value();
    auto u = r.read_le<std::uint32_t>(); if (!u) return core::fail(u.error());
    m.tag_unk = u.value();
    const std::size_t rem = r.remaining();
    const std::size_t payload_len = rem - 1;  // last byte is trailing
    auto pl = r.read_bytes(payload_len); if (!pl) return core::fail(pl.error());
    m.payload.resize(payload_len);
    for (std::size_t i = 0; i < payload_len; ++i) {
        m.payload[i] = static_cast<std::uint8_t>(pl.value()[i]);
    }
    auto tb = r.read_le<std::uint8_t>(); if (!tb) return core::fail(tb.error());
    m.trailing = tb.value();
    return m;
}

core::Result<AnonGameIconReply> dec_icon_reply(Reader& r) {
    AnonGameIconReply m;
    auto c = r.read_le<std::uint32_t>(); if (!c) return core::fail(c.error());
    m.count = c.value();
    auto cur = r.read_bytes(4); if (!cur) return core::fail(cur.error());
    for (std::size_t i = 0; i < 4; ++i) {
        m.curricon[i] = static_cast<char>(cur.value()[i]);
    }
    auto tw = r.read_le<std::uint8_t>(); if (!tw) return core::fail(tw.error());
    m.table_width = tw.value();
    auto ts = r.read_le<std::uint8_t>(); if (!ts) return core::fail(ts.error());
    m.table_size = ts.value();
    m.entries.reserve(m.table_size);
    for (std::uint8_t i = 0; i < m.table_size; ++i) {
        AnonGameIconReplyEntry e;
        auto ic = r.read_bytes(4); if (!ic) return core::fail(ic.error());
        for (std::size_t k = 0; k < 4; ++k) {
            e.icon_code[k] = static_cast<char>(ic.value()[k]);
        }
        auto pc = r.read_le<std::uint32_t>(); if (!pc) return core::fail(pc.error());
        e.portrait_code = pc.value();
        auto rc = r.read_le<std::uint8_t>(); if (!rc) return core::fail(rc.error());
        e.race = rc.value();
        // required_wins is BIG-endian on the wire (legacy bn_short_set).
        auto rw = r.read_be<std::uint16_t>(); if (!rw) return core::fail(rw.error());
        e.required_wins = rw.value();
        auto en = r.read_le<std::uint8_t>(); if (!en) return core::fail(en.error());
        e.client_enabled = en.value();
        m.entries.push_back(e);
    }
    return m;
}

core::Result<AnonGameProfileReply> dec_profile_reply(Reader& r) {
    AnonGameProfileReply m;
    auto c = r.read_le<std::uint32_t>(); if (!c) return core::fail(c.error());
    m.count = c.value();
    auto i = r.read_le<std::uint32_t>(); if (!i) return core::fail(i.error());
    m.icon = i.value();
    auto rc = r.read_le<std::uint8_t>(); if (!rc) return core::fail(rc.error());
    m.rescount = rc.value();
    auto tail = r.tail();
    m.data.resize(tail.size());
    for (std::size_t i2 = 0; i2 < tail.size(); ++i2) {
        m.data[i2] = static_cast<std::uint8_t>(tail[i2]);
    }
    return m;
}

core::Result<AnonGameTournamentReply> dec_tournament_reply(Reader& r) {
    AnonGameTournamentReply m;
    auto u32 = [&](std::uint32_t& dst) -> core::Status<> {
        auto v = r.read_le<std::uint32_t>(); if (!v) return core::fail(v.error());
        dst = v.value(); return core::ok();
    };
    auto u16 = [&](std::uint16_t& dst) -> core::Status<> {
        auto v = r.read_le<std::uint16_t>(); if (!v) return core::fail(v.error());
        dst = v.value(); return core::ok();
    };
    auto u8 = [&](std::uint8_t& dst) -> core::Status<> {
        auto v = r.read_le<std::uint8_t>(); if (!v) return core::fail(v.error());
        dst = v.value(); return core::ok();
    };
    if (auto s = u32(m.count); !s) return core::fail(s.error());
    if (auto s = u8 (m.type); !s) return core::fail(s.error());
    if (auto s = u8 (m.unknown1); !s) return core::fail(s.error());
    if (auto s = u16(m.unknown4); !s) return core::fail(s.error());
    if (auto s = u32(m.timestamp); !s) return core::fail(s.error());
    if (auto s = u8 (m.unknown5); !s) return core::fail(s.error());
    if (auto s = u16(m.countdown); !s) return core::fail(s.error());
    if (auto s = u16(m.unknown2); !s) return core::fail(s.error());
    if (auto s = u8 (m.wins); !s) return core::fail(s.error());
    if (auto s = u8 (m.losses); !s) return core::fail(s.error());
    if (auto s = u8 (m.ties); !s) return core::fail(s.error());
    if (auto s = u8 (m.unknown3); !s) return core::fail(s.error());
    if (auto s = u8 (m.selection); !s) return core::fail(s.error());
    if (auto s = u8 (m.descnum); !s) return core::fail(s.error());
    if (auto s = u8 (m.nulltag); !s) return core::fail(s.error());
    return m;
}

// ----- encoders ----------------------------------------------------------

void enc_search(Writer& w, const AnonGameSearch& m) {
    w.write_le<std::uint32_t>(m.count);
    w.write_le<std::uint32_t>(m.unknown2);
    w.write_le<std::uint8_t>(m.type);
    w.write_le<std::uint8_t>(m.gametype);
    w.write_le<std::uint32_t>(m.map_prefs);
    w.write_le<std::uint8_t>(m.unknown3);
    w.write_le<std::uint32_t>(m.id);
    w.write_le<std::uint32_t>(m.race);
}

void enc_at_search(Writer& w, const AnonGameAtSearch& m) {
    w.write_le<std::uint32_t>(m.count);
    w.write_le<std::uint32_t>(m.tid);
    w.write_le<std::uint32_t>(m.timestamp);
    w.write_le<std::uint8_t>(m.teamsize);
    for (auto v : m.info) w.write_le<std::uint32_t>(v);
    w.write_le<std::uint32_t>(m.unknown2);
    w.write_le<std::uint8_t>(m.unknown3);
    w.write_le<std::uint32_t>(m.id);
    w.write_le<std::uint32_t>(m.race);
}

void enc_at_inv(Writer& w, const AnonGameAtInviterSearch& m) {
    w.write_le<std::uint32_t>(m.count);
    w.write_le<std::uint32_t>(m.tid);
    w.write_le<std::uint32_t>(m.timestamp);
    w.write_le<std::uint8_t>(m.teamsize);
    for (auto v : m.info) w.write_le<std::uint32_t>(v);
    w.write_le<std::uint32_t>(m.unknown2);
    w.write_le<std::uint8_t>(m.type);
    w.write_le<std::uint8_t>(m.gametype);
    w.write_le<std::uint32_t>(m.map_prefs);
    w.write_le<std::uint8_t>(m.unknown3);
    w.write_le<std::uint32_t>(m.id);
    w.write_le<std::uint32_t>(m.race);
}

void enc_inforeq(Writer& w, const AnonGameInfoRequest& m) {
    w.write_le<std::uint32_t>(m.count);
    w.write_le<std::uint8_t>(m.noitems);
    for (const auto& e : m.entries) {
        w.write_le<std::uint32_t>(e.tag);
        w.write_le<std::uint32_t>(e.tag_unk);
    }
}

void enc_client_cancel(Writer& w, const AnonGameClientCancel& m) {
    w.write_le<std::uint32_t>(m.count);
}

void enc_profile_req(Writer& w, const AnonGameProfileRequest& m) {
    w.write_le<std::uint32_t>(m.count);
    w.write_cstring(m.username);
    w.write_cstring(m.client_tag);
}

void enc_tournament_req(Writer& w, const AnonGameTournamentRequest& m) {
    w.write_le<std::uint32_t>(m.count);
}

void enc_clan_profile(Writer& w, const AnonGameClanProfileRequest& m) {
    w.write_le<std::uint32_t>(m.count);
    w.write_le<std::uint32_t>(m.clan_tag);
}

void enc_get_icon(Writer& w, const AnonGameGetIcon& m) {
    w.write_le<std::uint32_t>(m.count);
}

void enc_set_icon(Writer& w, const AnonGameSetIcon& m) {
    w.write_le<std::uint32_t>(m.count);
    w.write_le<std::uint32_t>(m.icon);
}

void enc_search_reply(Writer& w, const AnonGameSearchReply& m) {
    w.write_le<std::uint32_t>(m.count);
    w.write_le<std::uint32_t>(m.reply);
}

void enc_found(Writer& w, const AnonGameFound& m) {
    w.write_le<std::uint32_t>(m.count);
    w.write_le<std::uint32_t>(m.unknown1);
    w.write_le<std::uint32_t>(m.ip_be);
    w.write_le<std::uint16_t>(m.port_be);
    w.write_le<std::uint8_t>(m.unknown2);
    w.write_le<std::uint8_t>(m.unknown3);
    w.write_le<std::uint16_t>(m.unknown4);
    w.write_le<std::uint32_t>(m.id);
    w.write_le<std::uint8_t>(m.unknown5);
    w.write_le<std::uint8_t>(m.type);
    w.write_le<std::uint8_t>(m.gametype);
    w.write_cstring(m.mapname);
    w.write_le<std::uint32_t>(m.saf.unknown1);
    w.write_le<std::uint32_t>(m.saf.anongame_string);
    w.write_le<std::uint8_t>(m.saf.totalplayers);
    w.write_le<std::uint8_t>(m.saf.totalteams);
    w.write_le<std::uint16_t>(m.saf.unknown2);
    w.write_le<std::uint8_t>(m.saf.visibility);
    w.write_le<std::uint8_t>(m.saf.unknown3);
    for (auto b : m.extras) w.write_le<std::uint8_t>(b);
}

void enc_server_cancel(Writer& w, const AnonGameServerCancel& m) {
    w.write_le<std::uint32_t>(m.count);
}

void enc_inforeply(Writer& w, const AnonGameInfoReply& m) {
    w.write_le<std::uint32_t>(m.count);
    w.write_le<std::uint8_t>(m.noitems);
    w.write_le<std::uint32_t>(m.tag);
    w.write_le<std::uint32_t>(m.tag_unk);
    for (auto b : m.payload) w.write_le<std::uint8_t>(b);
    w.write_le<std::uint8_t>(m.trailing);
}

void enc_profile_reply(Writer& w, const AnonGameProfileReply& m) {
    w.write_le<std::uint32_t>(m.count);
    w.write_le<std::uint32_t>(m.icon);
    w.write_le<std::uint8_t>(m.rescount);
    for (auto b : m.data) w.write_le<std::uint8_t>(b);
}

core::Result<AnonGameClanProfileReply> dec_clan_profile_reply(Reader& r) {
    AnonGameClanProfileReply m;
    auto c = r.read_le<std::uint32_t>(); if (!c) return core::fail(c.error());
    m.count = c.value();
    auto rc = r.read_le<std::uint8_t>(); if (!rc) return core::fail(rc.error());
    m.rescount = rc.value();
    auto tail = r.tail();
    m.trailer.resize(tail.size());
    for (std::size_t i = 0; i < tail.size(); ++i) {
        m.trailer[i] = static_cast<std::uint8_t>(tail[i]);
    }
    return m;
}

void enc_clan_profile_reply(Writer& w, const AnonGameClanProfileReply& m) {
    w.write_le<std::uint32_t>(m.count);
    w.write_le<std::uint8_t>(m.rescount);
    for (auto b : m.trailer) w.write_le<std::uint8_t>(b);
}

void enc_icon_reply(Writer& w, const AnonGameIconReply& m) {
    w.write_le<std::uint32_t>(m.count);
    for (auto c : m.curricon) w.write_le<std::uint8_t>(static_cast<std::uint8_t>(c));
    w.write_le<std::uint8_t>(m.table_width);
    w.write_le<std::uint8_t>(m.table_size);
    for (const auto& e : m.entries) {
        for (auto c : e.icon_code) {
            w.write_le<std::uint8_t>(static_cast<std::uint8_t>(c));
        }
        w.write_le<std::uint32_t>(e.portrait_code);
        w.write_le<std::uint8_t>(e.race);
        w.write_be<std::uint16_t>(e.required_wins);
        w.write_le<std::uint8_t>(e.client_enabled);
    }
}

void enc_tournament_reply(Writer& w, const AnonGameTournamentReply& m) {
    w.write_le<std::uint32_t>(m.count);
    w.write_le<std::uint8_t>(m.type);
    w.write_le<std::uint8_t>(m.unknown1);
    w.write_le<std::uint16_t>(m.unknown4);
    w.write_le<std::uint32_t>(m.timestamp);
    w.write_le<std::uint8_t>(m.unknown5);
    w.write_le<std::uint16_t>(m.countdown);
    w.write_le<std::uint16_t>(m.unknown2);
    w.write_le<std::uint8_t>(m.wins);
    w.write_le<std::uint8_t>(m.losses);
    w.write_le<std::uint8_t>(m.ties);
    w.write_le<std::uint8_t>(m.unknown3);
    w.write_le<std::uint8_t>(m.selection);
    w.write_le<std::uint8_t>(m.descnum);
    w.write_le<std::uint8_t>(m.nulltag);
}

}  // namespace

// ----- public API --------------------------------------------------------

core::Result<AnonGameClient> parse_findanongame_request(
    const WarcraftGeneralRequest& env) {
    Reader r{view_of(env.data)};
    switch (env.sub_option) {
        case kAnonGameClientSearch: {
            auto v = dec_search(r); if (!v) return core::fail(v.error());
            return AnonGameClient{std::move(v.value())};
        }
        case kAnonGameClientAtSearch: {
            auto v = dec_at_search(r); if (!v) return core::fail(v.error());
            return AnonGameClient{std::move(v.value())};
        }
        case kAnonGameClientAtInviterSearch: {
            auto v = dec_at_inv(r); if (!v) return core::fail(v.error());
            return AnonGameClient{std::move(v.value())};
        }
        case kAnonGameClientInfos: {
            auto v = dec_inforeq(r); if (!v) return core::fail(v.error());
            return AnonGameClient{std::move(v.value())};
        }
        case kAnonGameClientCancel: {
            auto v = dec_client_cancel(r); if (!v) return core::fail(v.error());
            return AnonGameClient{std::move(v.value())};
        }
        case kAnonGameClientProfile: {
            auto v = dec_profile_req(r); if (!v) return core::fail(v.error());
            return AnonGameClient{std::move(v.value())};
        }
        case kAnonGameClientTournament: {
            auto v = dec_tournament_req(r); if (!v) return core::fail(v.error());
            return AnonGameClient{std::move(v.value())};
        }
        case kAnonGameClientProfileClan: {
            auto v = dec_clan_profile(r); if (!v) return core::fail(v.error());
            return AnonGameClient{std::move(v.value())};
        }
        case kAnonGameClientGetIcon: {
            auto v = dec_get_icon(r); if (!v) return core::fail(v.error());
            return AnonGameClient{std::move(v.value())};
        }
        case kAnonGameClientSetIcon: {
            auto v = dec_set_icon(r); if (!v) return core::fail(v.error());
            return AnonGameClient{std::move(v.value())};
        }
        default:
            return core::fail(unimplemented_sub(env.sub_option, "client"));
    }
}

core::Result<AnonGameServer> parse_findanongame_reply(
    const WarcraftGeneralReply& env) {
    Reader r{view_of(env.data)};
    switch (env.sub_option) {
        case kAnonGameServerSearch: {
            auto v = dec_search_reply(r); if (!v) return core::fail(v.error());
            return AnonGameServer{std::move(v.value())};
        }
        case kAnonGameServerFound: {
            auto v = dec_found(r); if (!v) return core::fail(v.error());
            return AnonGameServer{std::move(v.value())};
        }
        case kAnonGameServerCancel: {
            auto v = dec_server_cancel(r); if (!v) return core::fail(v.error());
            return AnonGameServer{std::move(v.value())};
        }
        case kAnonGameClientInfos: {  // 0x02 = INFOREPLY (server)
            auto v = dec_inforeply(r); if (!v) return core::fail(v.error());
            return AnonGameServer{std::move(v.value())};
        }
        case kAnonGameClientProfile: {  // 0x04 = PROFILE2 (server)
            auto v = dec_profile_reply(r); if (!v) return core::fail(v.error());
            return AnonGameServer{std::move(v.value())};
        }
        case kAnonGameClientTournament: {  // 0x07 = TOURNAMENT (server)
            auto v = dec_tournament_reply(r); if (!v) return core::fail(v.error());
            return AnonGameServer{std::move(v.value())};
        }
        case kAnonGameClientGetIcon: {  // 0x09 = ICONREPLY (server)
            auto v = dec_icon_reply(r); if (!v) return core::fail(v.error());
            return AnonGameServer{std::move(v.value())};
        }
        case kAnonGameClientProfileClan: {  // 0x08 = CLAN_PROFILE (server)
            auto v = dec_clan_profile_reply(r); if (!v) return core::fail(v.error());
            return AnonGameServer{std::move(v.value())};
        }
        default:
            return core::fail(unimplemented_sub(env.sub_option, "server"));
    }
}

WarcraftGeneralRequest serialize_findanongame_request(const AnonGameClient& a) {
    WarcraftGeneralRequest env;
    Writer w;
    std::visit([&](const auto& m) {
        using T = std::decay_t<decltype(m)>;
        if constexpr (std::is_same_v<T, AnonGameSearch>) {
            env.sub_option = kAnonGameClientSearch; enc_search(w, m);
        } else if constexpr (std::is_same_v<T, AnonGameAtSearch>) {
            env.sub_option = kAnonGameClientAtSearch; enc_at_search(w, m);
        } else if constexpr (std::is_same_v<T, AnonGameAtInviterSearch>) {
            env.sub_option = kAnonGameClientAtInviterSearch; enc_at_inv(w, m);
        } else if constexpr (std::is_same_v<T, AnonGameInfoRequest>) {
            env.sub_option = kAnonGameClientInfos; enc_inforeq(w, m);
        } else if constexpr (std::is_same_v<T, AnonGameClientCancel>) {
            env.sub_option = kAnonGameClientCancel; enc_client_cancel(w, m);
        } else if constexpr (std::is_same_v<T, AnonGameProfileRequest>) {
            env.sub_option = kAnonGameClientProfile; enc_profile_req(w, m);
        } else if constexpr (std::is_same_v<T, AnonGameTournamentRequest>) {
            env.sub_option = kAnonGameClientTournament; enc_tournament_req(w, m);
        } else if constexpr (std::is_same_v<T, AnonGameClanProfileRequest>) {
            env.sub_option = kAnonGameClientProfileClan; enc_clan_profile(w, m);
        } else if constexpr (std::is_same_v<T, AnonGameGetIcon>) {
            env.sub_option = kAnonGameClientGetIcon; enc_get_icon(w, m);
        } else if constexpr (std::is_same_v<T, AnonGameSetIcon>) {
            env.sub_option = kAnonGameClientSetIcon; enc_set_icon(w, m);
        }
    }, a);
    env.data = take_bytes(std::move(w));
    return env;
}

WarcraftGeneralReply serialize_findanongame_reply(const AnonGameServer& a) {
    WarcraftGeneralReply env;
    Writer w;
    std::visit([&](const auto& m) {
        using T = std::decay_t<decltype(m)>;
        if constexpr (std::is_same_v<T, AnonGameSearchReply>) {
            env.sub_option = kAnonGameServerSearch; enc_search_reply(w, m);
        } else if constexpr (std::is_same_v<T, AnonGameFound>) {
            env.sub_option = kAnonGameServerFound; enc_found(w, m);
        } else if constexpr (std::is_same_v<T, AnonGameServerCancel>) {
            env.sub_option = kAnonGameServerCancel; enc_server_cancel(w, m);
        } else if constexpr (std::is_same_v<T, AnonGameInfoReply>) {
            env.sub_option = kAnonGameClientInfos; enc_inforeply(w, m);
        } else if constexpr (std::is_same_v<T, AnonGameProfileReply>) {
            env.sub_option = kAnonGameClientProfile; enc_profile_reply(w, m);
        } else if constexpr (std::is_same_v<T, AnonGameTournamentReply>) {
            env.sub_option = kAnonGameClientTournament; enc_tournament_reply(w, m);
        } else if constexpr (std::is_same_v<T, AnonGameIconReply>) {
            env.sub_option = kAnonGameClientGetIcon; enc_icon_reply(w, m);
        } else if constexpr (std::is_same_v<T, AnonGameClanProfileReply>) {
            env.sub_option = kAnonGameClientProfileClan; enc_clan_profile_reply(w, m);
        }
    }, a);
    env.data = take_bytes(std::move(w));
    return env;
}

}  // namespace pvpgn::protocol::bnet
