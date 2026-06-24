// SPDX-License-Identifier: GPL-2.0-or-later
//
// End-to-end tests for the FINDANONGAME INFOREPLY composition service.
// Verifies: tag mapping, single-reply round-trip (compress -> in-place
// inflate -> typed parse), full INFOREQ -> N replies trailing-flag
// behaviour, and the "snapshot missing payload" path.

#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "application/anongame_inforeply/inforeply_builder.hpp"
#include "infra/compression/zlib_anongame.hpp"
#include "infra/compression/zlib_anongame_compressor.hpp"
#include "protocol/bnet/anongame.hpp"
#include "protocol/bnet/anongame_tags.hpp"

using namespace pvpgn;
using namespace pvpgn::application::anongame_inforeply;

namespace pb = pvpgn::protocol::bnet;

namespace {

// R217 follow-up: the application no longer depends on infra at compile
// time; the test constructs the concrete zlib adapter and passes it as
// the `IAnonGameCompressor` port.
const infra::compression::ZlibAnonGameCompressor kCompressor{};

AnonGameInfoSnapshot make_full_snapshot() {
    AnonGameInfoSnapshot s{};
    s.url = pb::AnonGameUrlPayload{{"http://server", "http://player",
                                    "http://tourney", "http://clan"}};
    s.map = pb::AnonGameMapPayload{{
        "Maps\\FrozenThrone\\(4)LostTemple.w3x",
        "Maps\\FrozenThrone\\(6)GnollWood.w3x",
    }};
    pb::AnonGameTypeSection pg{};
    pg.section_id = 0;
    pg.gamestyles = {{{0x00, 0x00, 0x03, 0x3F, 0x00}, {0, 1}}};
    s.type = pb::AnonGameTypePayload{{pg}};
    s.desc = pb::AnonGameDescPayload{{
        {0, 0, "1v1", "One vs. One"},
        {0, 1, "2v2", "Two vs. Two"},
    }};
    s.ladr = pb::AnonGameLadrPayload{{
        {0x534F4C4Fu, "PG 1v1", "http://l/pg1v1"},
        {0x5445414Du, "PG team", "http://l/pgteam"},
    }};
    return s;
}

}  // namespace

TEST_CASE("inforeply: server_tag_for maps the five known tags",
          "[application][anongame_inforeply]") {
    REQUIRE(server_tag_for(pb::kAnonGameInfoTagURL).value()  == pb::kAnonGameInfoTagServerURL);
    REQUIRE(server_tag_for(pb::kAnonGameInfoTagMAP).value()  == pb::kAnonGameInfoTagServerMAP);
    REQUIRE(server_tag_for(pb::kAnonGameInfoTagTYPE).value() == pb::kAnonGameInfoTagServerTYPE);
    REQUIRE(server_tag_for(pb::kAnonGameInfoTagDESC).value() == pb::kAnonGameInfoTagServerDESC);
    REQUIRE(server_tag_for(pb::kAnonGameInfoTagLADR).value() == pb::kAnonGameInfoTagServerLADR);
}

TEST_CASE("inforeply: server_tag_for rejects unknown tag",
          "[application][anongame_inforeply]") {
    auto r = server_tag_for(0xDEADBEEFu);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("inforeply: URL single-tag round-trip",
          "[application][anongame_inforeply]") {
    auto snap = make_full_snapshot();
    auto reply = build_inforeply_for_tag(
        pb::kAnonGameInfoTagURL, /*tag_unk=*/0xCAFEBABEu, /*count=*/42,
        snap, /*more=*/true, kCompressor);
    REQUIRE(reply.has_value());
    REQUIRE(reply.value().count    == 42);
    REQUIRE(reply.value().noitems  == 1);
    REQUIRE(reply.value().tag      == pb::kAnonGameInfoTagServerURL);
    REQUIRE(reply.value().tag_unk  == 0xCAFEBABEu);
    REQUIRE(reply.value().trailing == 0x01);

    // Decompress + reparse and confirm equality with the snapshot payload.
    auto raw = infra::compression::anongame_decompress(
        std::span<const std::uint8_t>{reply.value().payload.data(),
                                      reply.value().payload.size()});
    REQUIRE(raw.has_value());
    auto parsed = pb::parse_url_payload(raw.value(), /*expected_count=*/4);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed.value() == snap.url.value());
}

TEST_CASE("inforeply: MAP / TYPE / DESC / LADR round-trips",
          "[application][anongame_inforeply]") {
    auto snap = make_full_snapshot();

    {  // MAP
        auto r = build_inforeply_for_tag(pb::kAnonGameInfoTagMAP, 0, 1, snap, false, kCompressor);
        REQUIRE(r.has_value());
        REQUIRE(r.value().tag == pb::kAnonGameInfoTagServerMAP);
        auto raw = infra::compression::anongame_decompress(
            std::span<const std::uint8_t>{r.value().payload.data(), r.value().payload.size()});
        REQUIRE(raw.has_value());
        auto p = pb::parse_map_payload(raw.value());
        REQUIRE(p.has_value());
        REQUIRE(p.value() == snap.map.value());
    }
    {  // TYPE
        auto r = build_inforeply_for_tag(pb::kAnonGameInfoTagTYPE, 0, 1, snap, false, kCompressor);
        REQUIRE(r.has_value());
        REQUIRE(r.value().tag == pb::kAnonGameInfoTagServerTYPE);
        auto raw = infra::compression::anongame_decompress(
            std::span<const std::uint8_t>{r.value().payload.data(), r.value().payload.size()});
        REQUIRE(raw.has_value());
        auto p = pb::parse_type_payload(raw.value());
        REQUIRE(p.has_value());
        REQUIRE(p.value() == snap.type.value());
    }
    {  // DESC
        auto r = build_inforeply_for_tag(pb::kAnonGameInfoTagDESC, 0, 1, snap, false, kCompressor);
        REQUIRE(r.has_value());
        REQUIRE(r.value().tag == pb::kAnonGameInfoTagServerDESC);
        auto raw = infra::compression::anongame_decompress(
            std::span<const std::uint8_t>{r.value().payload.data(), r.value().payload.size()});
        REQUIRE(raw.has_value());
        auto p = pb::parse_desc_payload(raw.value());
        REQUIRE(p.has_value());
        REQUIRE(p.value() == snap.desc.value());
    }
    {  // LADR
        auto r = build_inforeply_for_tag(pb::kAnonGameInfoTagLADR, 0, 1, snap, false, kCompressor);
        REQUIRE(r.has_value());
        REQUIRE(r.value().tag == pb::kAnonGameInfoTagServerLADR);
        auto raw = infra::compression::anongame_decompress(
            std::span<const std::uint8_t>{r.value().payload.data(), r.value().payload.size()});
        REQUIRE(raw.has_value());
        auto p = pb::parse_ladr_payload(raw.value());
        REQUIRE(p.has_value());
        REQUIRE(p.value() == snap.ladr.value());
    }
}

TEST_CASE("inforeply: missing snapshot payload -> NotFound",
          "[application][anongame_inforeply]") {
    AnonGameInfoSnapshot empty;
    auto r = build_inforeply_for_tag(
        pb::kAnonGameInfoTagURL, 0, 1, empty, false, kCompressor);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::NotFound);
}

TEST_CASE("inforeply: full INFOREQ produces ordered replies with trailing flags",
          "[application][anongame_inforeply]") {
    auto snap = make_full_snapshot();
    pb::AnonGameInfoRequest req{};
    req.count   = 7;
    req.noitems = 3;
    req.entries = {
        {pb::kAnonGameInfoTagURL,  0x01010101u},
        {pb::kAnonGameInfoTagMAP,  0x02020202u},
        {pb::kAnonGameInfoTagDESC, 0x03030303u},
    };
    auto replies = build_inforeplies_for_request(req, snap, kCompressor);
    REQUIRE(replies.has_value());
    REQUIRE(replies.value().size() == 3);

    REQUIRE(replies.value()[0].tag      == pb::kAnonGameInfoTagServerURL);
    REQUIRE(replies.value()[0].tag_unk  == 0x01010101u);
    REQUIRE(replies.value()[0].trailing == 0x01);
    REQUIRE(replies.value()[1].tag      == pb::kAnonGameInfoTagServerMAP);
    REQUIRE(replies.value()[1].tag_unk  == 0x02020202u);
    REQUIRE(replies.value()[1].trailing == 0x01);
    REQUIRE(replies.value()[2].tag      == pb::kAnonGameInfoTagServerDESC);
    REQUIRE(replies.value()[2].tag_unk  == 0x03030303u);
    REQUIRE(replies.value()[2].trailing == 0x00);

    for (const auto& r : replies.value()) {
        REQUIRE(r.count   == 7);
        REQUIRE(r.noitems == 1);
    }
}

TEST_CASE("inforeply: missing tags in snapshot are silently skipped",
          "[application][anongame_inforeply]") {
    AnonGameInfoSnapshot partial;
    partial.url = pb::AnonGameUrlPayload{{"a", "b", "c"}};
    partial.map = pb::AnonGameMapPayload{{"only-map"}};
    // TYPE / DESC / LADR intentionally missing.

    pb::AnonGameInfoRequest req{};
    req.count   = 1;
    req.noitems = 5;
    req.entries = {
        {pb::kAnonGameInfoTagURL,  0},
        {pb::kAnonGameInfoTagTYPE, 0},  // skipped
        {pb::kAnonGameInfoTagMAP,  0},
        {pb::kAnonGameInfoTagDESC, 0},  // skipped
        {pb::kAnonGameInfoTagLADR, 0},  // skipped
    };
    auto replies = build_inforeplies_for_request(req, partial, kCompressor);
    REQUIRE(replies.has_value());
    REQUIRE(replies.value().size() == 2);
    REQUIRE(replies.value()[0].tag      == pb::kAnonGameInfoTagServerURL);
    REQUIRE(replies.value()[0].trailing == 0x01);
    REQUIRE(replies.value()[1].tag      == pb::kAnonGameInfoTagServerMAP);
    REQUIRE(replies.value()[1].trailing == 0x00);
}

TEST_CASE("inforeply: empty request yields empty reply set",
          "[application][anongame_inforeply]") {
    pb::AnonGameInfoRequest req{};
    req.count   = 0;
    req.noitems = 0;
    auto replies = build_inforeplies_for_request(req, make_full_snapshot(), kCompressor);
    REQUIRE(replies.has_value());
    REQUIRE(replies.value().empty());
}

TEST_CASE("inforeply: unknown tag in request -> InvalidArgument",
          "[application][anongame_inforeply]") {
    pb::AnonGameInfoRequest req{};
    req.count   = 1;
    req.noitems = 1;
    req.entries = {{0xDEADBEEFu, 0}};
    auto replies = build_inforeplies_for_request(req, make_full_snapshot(), kCompressor);
    REQUIRE_FALSE(replies.has_value());
    REQUIRE(replies.error().code() == core::StatusCode::InvalidArgument);
}

// =========================================================================
// SID 0x44 packet encoding
// =========================================================================

TEST_CASE("inforeply: encode_inforeply_packet starts with 0xFF 0x44 + length",
          "[application][anongame_inforeply]") {
    auto snap = make_full_snapshot();
    auto reply = build_inforeply_for_tag(
        pb::kAnonGameInfoTagURL, 0, 1, snap, false, kCompressor);
    REQUIRE(reply.has_value());
    auto bytes = encode_inforeply_packet(reply.value());
    REQUIRE(bytes.has_value());
    REQUIRE(bytes.value().size() >= 5);
    REQUIRE(std::to_integer<std::uint8_t>(bytes.value()[0]) == 0xFF);
    REQUIRE(std::to_integer<std::uint8_t>(bytes.value()[1]) == 0x44);
    const std::uint16_t len = static_cast<std::uint16_t>(
        std::to_integer<std::uint16_t>(bytes.value()[2]) |
        (std::to_integer<std::uint16_t>(bytes.value()[3]) << 8));
    REQUIRE(len == bytes.value().size());
    // sub_option byte for INFOREPLY is the client INFOS code 0x02.
    REQUIRE(std::to_integer<std::uint8_t>(bytes.value()[4]) ==
            pb::kAnonGameClientInfos);
}

TEST_CASE("inforeply: encode_inforeply_packet round-trips via parse",
          "[application][anongame_inforeply]") {
    auto snap = make_full_snapshot();
    auto reply = build_inforeply_for_tag(
        pb::kAnonGameInfoTagMAP, 0xAA, 9, snap, true, kCompressor);
    REQUIRE(reply.has_value());
    auto bytes = encode_inforeply_packet(reply.value());
    REQUIRE(bytes.has_value());

    // Skip the 4-byte BNet header and re-parse the envelope.
    const auto& bs = bytes.value();
    REQUIRE(bs.size() >= 5);
    pb::WarcraftGeneralReply wgr{};
    wgr.sub_option = std::to_integer<std::uint8_t>(bs[4]);
    wgr.data.assign(bs.begin() + 5, bs.end());
    auto parsed = pb::parse_findanongame_reply(wgr);
    REQUIRE(parsed.has_value());
    REQUIRE(std::holds_alternative<pb::AnonGameInfoReply>(parsed.value()));
    REQUIRE(std::get<pb::AnonGameInfoReply>(parsed.value()) == reply.value());
}

TEST_CASE("inforeply: encode_inforeply_packets concatenates N packets",
          "[application][anongame_inforeply]") {
    auto snap = make_full_snapshot();
    pb::AnonGameInfoRequest req{};
    req.count   = 1;
    req.noitems = 3;
    req.entries = {
        {pb::kAnonGameInfoTagURL,  0},
        {pb::kAnonGameInfoTagMAP,  0},
        {pb::kAnonGameInfoTagLADR, 0},
    };
    auto bytes = encode_inforeplies_for_request(req, snap, kCompressor);
    REQUIRE(bytes.has_value());

    // Walk the byte stream and confirm we see exactly 3 0x44 packets
    // whose lengths cover the whole buffer.
    std::size_t cursor = 0;
    int packet_count = 0;
    while (cursor < bytes.value().size()) {
        REQUIRE(bytes.value().size() - cursor >= 4);
        REQUIRE(std::to_integer<std::uint8_t>(bytes.value()[cursor]) == 0xFF);
        REQUIRE(std::to_integer<std::uint8_t>(bytes.value()[cursor + 1]) == 0x44);
        const std::uint16_t len = static_cast<std::uint16_t>(
            std::to_integer<std::uint16_t>(bytes.value()[cursor + 2]) |
            (std::to_integer<std::uint16_t>(bytes.value()[cursor + 3]) << 8));
        REQUIRE(len >= 5);
        REQUIRE(cursor + len <= bytes.value().size());
        cursor += len;
        ++packet_count;
    }
    REQUIRE(packet_count == 3);
    REQUIRE(cursor == bytes.value().size());
}

TEST_CASE("inforeply: encode_inforeplies_for_request empty -> empty bytes",
          "[application][anongame_inforeply]") {
    AnonGameInfoSnapshot empty;
    pb::AnonGameInfoRequest req{};
    req.count   = 1;
    req.noitems = 1;
    req.entries = {{pb::kAnonGameInfoTagURL, 0}};  // not in snapshot
    auto bytes = encode_inforeplies_for_request(req, empty, kCompressor);
    REQUIRE(bytes.has_value());
    REQUIRE(bytes.value().empty());
}

// =========================================================================
// CompiledSnapshot caching layer
// =========================================================================

TEST_CASE("inforeply: compile_snapshot populates only the present tags",
          "[application][anongame_inforeply][compiled]") {
    AnonGameInfoSnapshot partial;
    partial.url = pb::AnonGameUrlPayload{{"a", "b", "c"}};
    partial.ladr = pb::AnonGameLadrPayload{{{0x534F4C4Fu, "d", "u"}}};
    auto compiled = compile_snapshot(partial, kCompressor);
    REQUIRE(compiled.has_value());
    REQUIRE(compiled.value().url.has_value());
    REQUIRE_FALSE(compiled.value().map.has_value());
    REQUIRE_FALSE(compiled.value().type.has_value());
    REQUIRE_FALSE(compiled.value().desc.has_value());
    REQUIRE(compiled.value().ladr.has_value());
    // Framed bytes start with the legacy 4-byte length header.
    REQUIRE(compiled.value().url.value().size() >= 4);
}

TEST_CASE("inforeply: compiled and snapshot paths produce identical packet bytes",
          "[application][anongame_inforeply][compiled]") {
    auto snap = make_full_snapshot();
    auto compiled = compile_snapshot(snap, kCompressor);
    REQUIRE(compiled.has_value());

    pb::AnonGameInfoRequest req{};
    req.count   = 11;
    req.noitems = 5;
    req.entries = {
        {pb::kAnonGameInfoTagURL,  0xAAAAAAAAu},
        {pb::kAnonGameInfoTagMAP,  0xBBBBBBBBu},
        {pb::kAnonGameInfoTagTYPE, 0xCCCCCCCCu},
        {pb::kAnonGameInfoTagDESC, 0xDDDDDDDDu},
        {pb::kAnonGameInfoTagLADR, 0xEEEEEEEEu},
    };
    auto from_snap     = encode_inforeplies_for_request(req, snap, kCompressor);
    auto from_compiled = encode_inforeplies_for_request(req, compiled.value());
    REQUIRE(from_snap.has_value());
    REQUIRE(from_compiled.has_value());
    // Catch2 has no default StringMaker for std::byte; compare sizes
    // and bytes via uint8_t to keep the diagnostic clean.
    REQUIRE(from_snap.value().size() == from_compiled.value().size());
    for (std::size_t i = 0; i < from_snap.value().size(); ++i) {
        REQUIRE(std::to_integer<std::uint8_t>(from_snap.value()[i]) ==
                std::to_integer<std::uint8_t>(from_compiled.value()[i]));
    }
}

TEST_CASE("inforeply: compiled build_inforeply_for_tag missing -> NotFound",
          "[application][anongame_inforeply][compiled]") {
    CompiledSnapshot empty;
    auto r = build_inforeply_for_tag(
        pb::kAnonGameInfoTagURL, 0, 1, empty, false);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::NotFound);
}

TEST_CASE("inforeply: compiled unknown tag -> InvalidArgument",
          "[application][anongame_inforeply][compiled]") {
    CompiledSnapshot compiled;
    auto r = build_inforeply_for_tag(0xDEADBEEFu, 0, 1, compiled, false);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("inforeply: compiled fan-out skips missing tags & sets trailing flags",
          "[application][anongame_inforeply][compiled]") {
    AnonGameInfoSnapshot partial;
    partial.url = pb::AnonGameUrlPayload{{"a", "b", "c"}};
    partial.map = pb::AnonGameMapPayload{{"only-map"}};
    auto compiled = compile_snapshot(partial, kCompressor);
    REQUIRE(compiled.has_value());

    pb::AnonGameInfoRequest req{};
    req.count   = 1;
    req.noitems = 5;
    req.entries = {
        {pb::kAnonGameInfoTagURL,  0},
        {pb::kAnonGameInfoTagTYPE, 0},  // skipped
        {pb::kAnonGameInfoTagMAP,  0},
        {pb::kAnonGameInfoTagDESC, 0},  // skipped
        {pb::kAnonGameInfoTagLADR, 0},  // skipped
    };
    auto replies = build_inforeplies_for_request(req, compiled.value());
    REQUIRE(replies.has_value());
    REQUIRE(replies.value().size() == 2);
    REQUIRE(replies.value()[0].tag      == pb::kAnonGameInfoTagServerURL);
    REQUIRE(replies.value()[0].trailing == 0x01);
    REQUIRE(replies.value()[1].tag      == pb::kAnonGameInfoTagServerMAP);
    REQUIRE(replies.value()[1].trailing == 0x00);
}

// =========================================================================
// CompiledSnapshotSet (multi-locale cache)
// =========================================================================

TEST_CASE("inforeply: compile_snapshot_set compiles default and all locales",
          "[application][anongame_inforeply][compiled][multilocale]") {
    AnonGameInfoSnapshot def;
    def.url  = pb::AnonGameUrlPayload{{"a", "b"}};
    def.desc = pb::AnonGameDescPayload{
        {{0, 0, "1v1", "One vs One"}}};

    AnonGameInfoSnapshot de = def;
    de.desc = pb::AnonGameDescPayload{
        {{0, 0, "1v1", "Eins gegen Eins"}}};

    AnonGameInfoSnapshot ru = def;
    ru.desc = pb::AnonGameDescPayload{
        {{0, 0, "1v1", "Odin na odin"}}};

    std::unordered_map<std::string, AnonGameInfoSnapshot> by_lang;
    by_lang.emplace("deDE", de);
    by_lang.emplace("ruRU", ru);

    auto set = compile_snapshot_set(def, by_lang, kCompressor);
    REQUIRE(set.has_value());
    REQUIRE(set.value().default_snapshot.url.has_value());
    REQUIRE(set.value().default_snapshot.desc.has_value());
    REQUIRE(set.value().by_lang.size() == 2);
    REQUIRE(set.value().by_lang.contains("deDE"));
    REQUIRE(set.value().by_lang.contains("ruRU"));

    // Each locale's compiled URL bytes match the default (same input);
    // DESC bytes differ because the strings differ.
    const auto& d = set.value().default_snapshot;
    const auto& l = set.value().by_lang.at("deDE");
    REQUIRE(l.url.value()  == d.url.value());
    REQUIRE(l.desc.value() != d.desc.value());
}

TEST_CASE("inforeply: CompiledSnapshotSet::select returns locale or default",
          "[application][anongame_inforeply][compiled][multilocale]") {
    AnonGameInfoSnapshot def;
    def.url = pb::AnonGameUrlPayload{{"default-url"}};

    AnonGameInfoSnapshot de;
    de.url = pb::AnonGameUrlPayload{{"de-url"}};

    std::unordered_map<std::string, AnonGameInfoSnapshot> by_lang;
    by_lang.emplace("deDE", de);

    auto set = compile_snapshot_set(def, by_lang, kCompressor);
    REQUIRE(set.has_value());

    const auto& s_de  = set.value().select("deDE");
    const auto& s_ru  = set.value().select("ruRU");      // miss -> default
    const auto& s_def = set.value().default_snapshot;

    REQUIRE(s_de.url.value() != s_def.url.value());
    REQUIRE(&s_ru == &s_def);                            // exact same object
}

TEST_CASE("inforeply: compile_snapshot_set with empty locale map mirrors default",
          "[application][anongame_inforeply][compiled][multilocale]") {
    AnonGameInfoSnapshot def;
    def.url = pb::AnonGameUrlPayload{{"only"}};

    auto set = compile_snapshot_set(def, {}, kCompressor);
    REQUIRE(set.has_value());
    REQUIRE(set.value().by_lang.empty());
    REQUIRE(&set.value().select("anything") ==
            &set.value().default_snapshot);
}

TEST_CASE("inforeply: CompiledSnapshotSet feeds existing compiled-API encode path",
          "[application][anongame_inforeply][compiled][multilocale]") {
    AnonGameInfoSnapshot def;
    def.url = pb::AnonGameUrlPayload{{"u"}};

    auto set = compile_snapshot_set(def, {}, kCompressor);
    REQUIRE(set.has_value());

    pb::AnonGameInfoRequest req{};
    req.count   = 7;
    req.noitems = 1;
    req.entries = {{pb::kAnonGameInfoTagURL, 0u}};

    auto bytes = encode_inforeplies_for_request(
        req, set.value().select("anything"));
    REQUIRE(bytes.has_value());
    REQUIRE_FALSE(bytes.value().empty());
}
