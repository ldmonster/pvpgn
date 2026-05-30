// SPDX-License-Identifier: GPL-2.0-or-later
//
// Client-side sub-message decoders and encoders for SID_WARCRAFTGENERAL (0x44).
// Included directly into anongame.cpp inside namespace pvpgn::protocol::bnet.

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

// ----- client encoders --------------------------------------------------

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
