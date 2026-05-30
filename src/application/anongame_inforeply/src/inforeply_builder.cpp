// SPDX-License-Identifier: GPL-2.0-or-later

#include "application/anongame_inforeply/inforeply_builder.hpp"

#include <utility>

#include "protocol/bnet/codec.hpp"
#include "protocol/common/writer.hpp"

namespace pvpgn::application::anongame_inforeply {

namespace pb = pvpgn::protocol::bnet;

core::Result<std::uint32_t> server_tag_for(std::uint32_t client_tag) {
    switch (client_tag) {
        case pb::kAnonGameInfoTagURL:  return pb::kAnonGameInfoTagServerURL;
        case pb::kAnonGameInfoTagMAP:  return pb::kAnonGameInfoTagServerMAP;
        case pb::kAnonGameInfoTagTYPE: return pb::kAnonGameInfoTagServerTYPE;
        case pb::kAnonGameInfoTagDESC: return pb::kAnonGameInfoTagServerDESC;
        case pb::kAnonGameInfoTagLADR: return pb::kAnonGameInfoTagServerLADR;
        default:
            return core::fail(core::make_error(
                core::StatusCode::InvalidArgument,
                "anongame_inforeply: unknown client tag"));
    }
}

core::Result<pb::AnonGameInfoReply> compose_inforeply(
    std::uint32_t server_tag,
    std::uint32_t tag_unk,
    std::uint32_t count,
    std::span<const std::uint8_t> serialized_payload,
    bool more,
    const ports::IAnonGameCompressor& compressor) {
    auto framed = compressor.compress(serialized_payload);
    if (!framed) return core::fail(framed.error());

    pb::AnonGameInfoReply out{};
    out.count    = count;
    out.noitems  = 1;
    out.tag      = server_tag;
    out.tag_unk  = tag_unk;
    out.payload  = std::move(framed).value();
    out.trailing = more ? std::uint8_t{0x01} : std::uint8_t{0x00};
    return out;
}

namespace {

// Returns true if the snapshot has a payload for `client_tag`, and
// writes the serialized bytes into `out` on success. Returns false
// (with `out` left empty) if the snapshot has no payload for that
// tag. Returns nullopt on unknown tag.
struct SerializeResult {
    bool                       found = false;
    std::vector<std::uint8_t>  bytes;
};

core::Result<SerializeResult> serialize_from_snapshot(
    std::uint32_t client_tag, const AnonGameInfoSnapshot& s) {
    SerializeResult r;
    switch (client_tag) {
        case pb::kAnonGameInfoTagURL:
            if (!s.url) return r;
            r.found = true;
            r.bytes = pb::serialize_url_payload(*s.url);
            return r;
        case pb::kAnonGameInfoTagMAP:
            if (!s.map) return r;
            r.found = true;
            r.bytes = pb::serialize_map_payload(*s.map);
            return r;
        case pb::kAnonGameInfoTagTYPE:
            if (!s.type) return r;
            r.found = true;
            r.bytes = pb::serialize_type_payload(*s.type);
            return r;
        case pb::kAnonGameInfoTagDESC:
            if (!s.desc) return r;
            r.found = true;
            r.bytes = pb::serialize_desc_payload(*s.desc);
            return r;
        case pb::kAnonGameInfoTagLADR:
            if (!s.ladr) return r;
            r.found = true;
            r.bytes = pb::serialize_ladr_payload(*s.ladr);
            return r;
        default:
            return core::fail(core::make_error(
                core::StatusCode::InvalidArgument,
                "anongame_inforeply: unknown client tag"));
    }
}

}  // namespace

core::Result<pb::AnonGameInfoReply> build_inforeply_for_tag(
    std::uint32_t client_tag,
    std::uint32_t tag_unk,
    std::uint32_t count,
    const AnonGameInfoSnapshot& snapshot,
    bool more,
    const ports::IAnonGameCompressor& compressor) {
    auto ser = serialize_from_snapshot(client_tag, snapshot);
    if (!ser) return core::fail(ser.error());
    if (!ser.value().found) {
        return core::fail(core::make_error(
            core::StatusCode::NotFound,
            "anongame_inforeply: snapshot has no payload for requested tag"));
    }
    auto stag = server_tag_for(client_tag);
    if (!stag) return core::fail(stag.error());

    const auto& bytes = ser.value().bytes;
    return compose_inforeply(
        stag.value(), tag_unk, count,
        std::span<const std::uint8_t>{bytes.data(), bytes.size()}, more,
        compressor);
}

core::Result<std::vector<pb::AnonGameInfoReply>>
build_inforeplies_for_request(
    const pb::AnonGameInfoRequest& request,
    const AnonGameInfoSnapshot& snapshot,
    const ports::IAnonGameCompressor& compressor) {
    // First pass: figure out which requested tags actually have data
    // in the snapshot, preserving request order.
    std::vector<std::size_t> producing;
    producing.reserve(request.entries.size());
    for (std::size_t i = 0; i < request.entries.size(); ++i) {
        auto ser = serialize_from_snapshot(request.entries[i].tag, snapshot);
        if (!ser) return core::fail(ser.error());
        if (ser.value().found) producing.push_back(i);
    }

    std::vector<pb::AnonGameInfoReply> out;
    out.reserve(producing.size());
    for (std::size_t k = 0; k < producing.size(); ++k) {
        const auto& e    = request.entries[producing[k]];
        const bool  more = (k + 1 < producing.size());
        auto reply = build_inforeply_for_tag(
            e.tag, e.tag_unk, request.count, snapshot, more, compressor);
        if (!reply) return core::fail(reply.error());
        out.push_back(std::move(reply).value());
    }
    return out;
}

core::Result<std::vector<std::byte>> encode_inforeply_packet(
    const pb::AnonGameInfoReply& reply) {
    auto wgr = pb::serialize_findanongame_reply(pb::AnonGameServer{reply});
    pvpgn::protocol::Writer w;
    if (auto s = pb::encode(w, wgr); !s) return core::fail(s.error());
    return w.take();
}

core::Result<std::vector<std::byte>> encode_inforeply_packets(
    std::span<const pb::AnonGameInfoReply> replies) {
    pvpgn::protocol::Writer w;
    for (const auto& r : replies) {
        auto wgr = pb::serialize_findanongame_reply(pb::AnonGameServer{r});
        if (auto s = pb::encode(w, wgr); !s) return core::fail(s.error());
    }
    return w.take();
}

core::Result<std::vector<std::byte>> encode_inforeplies_for_request(
    const pb::AnonGameInfoRequest& request,
    const AnonGameInfoSnapshot& snapshot,
    const ports::IAnonGameCompressor& compressor) {
    auto replies = build_inforeplies_for_request(request, snapshot, compressor);
    if (!replies) return core::fail(replies.error());
    return encode_inforeply_packets(
        std::span<const pb::AnonGameInfoReply>{replies.value().data(),
                                                replies.value().size()});
}

// =========================================================================
// CompiledSnapshot machinery
// =========================================================================

namespace {

core::Result<std::vector<std::uint8_t>> compress_one(
    const std::vector<std::uint8_t>& serialized,
    const ports::IAnonGameCompressor& compressor) {
    return compressor.compress(
        std::span<const std::uint8_t>{serialized.data(), serialized.size()});
}

// Look up the pre-compressed framed bytes for a given client tag.
// Returns nullptr when the compiled snapshot has no data for that
// tag, or InvalidArgument for unknown tags.
core::Result<const std::vector<std::uint8_t>*> lookup_compiled(
    std::uint32_t client_tag, const CompiledSnapshot& c) {
    switch (client_tag) {
        case pb::kAnonGameInfoTagURL:
            return c.url ? &*c.url : nullptr;
        case pb::kAnonGameInfoTagMAP:
            return c.map ? &*c.map : nullptr;
        case pb::kAnonGameInfoTagTYPE:
            return c.type ? &*c.type : nullptr;
        case pb::kAnonGameInfoTagDESC:
            return c.desc ? &*c.desc : nullptr;
        case pb::kAnonGameInfoTagLADR:
            return c.ladr ? &*c.ladr : nullptr;
        default:
            return core::fail(core::make_error(
                core::StatusCode::InvalidArgument,
                "anongame_inforeply: unknown client tag"));
    }
}

}  // namespace

core::Result<CompiledSnapshot> compile_snapshot(
    const AnonGameInfoSnapshot& s,
    const ports::IAnonGameCompressor& compressor) {
    CompiledSnapshot c{};
    if (s.url) {
        auto bytes = compress_one(pb::serialize_url_payload(*s.url), compressor);
        if (!bytes) return core::fail(bytes.error());
        c.url = std::move(bytes).value();
    }
    if (s.map) {
        auto bytes = compress_one(pb::serialize_map_payload(*s.map), compressor);
        if (!bytes) return core::fail(bytes.error());
        c.map = std::move(bytes).value();
    }
    if (s.type) {
        auto bytes = compress_one(pb::serialize_type_payload(*s.type), compressor);
        if (!bytes) return core::fail(bytes.error());
        c.type = std::move(bytes).value();
    }
    if (s.desc) {
        auto bytes = compress_one(pb::serialize_desc_payload(*s.desc), compressor);
        if (!bytes) return core::fail(bytes.error());
        c.desc = std::move(bytes).value();
    }
    if (s.ladr) {
        auto bytes = compress_one(pb::serialize_ladr_payload(*s.ladr), compressor);
        if (!bytes) return core::fail(bytes.error());
        c.ladr = std::move(bytes).value();
    }
    return c;
}

const CompiledSnapshot& CompiledSnapshotSet::select(
    std::string_view lang_id) const {
    auto it = by_lang.find(std::string{lang_id});
    if (it == by_lang.end()) return default_snapshot;
    return it->second;
}

core::Result<CompiledSnapshotSet> compile_snapshot_set(
    const AnonGameInfoSnapshot& default_snapshot,
    const std::unordered_map<std::string, AnonGameInfoSnapshot>& by_lang,
    const ports::IAnonGameCompressor& compressor) {
    CompiledSnapshotSet out{};
    auto def = compile_snapshot(default_snapshot, compressor);
    if (!def) return core::fail(def.error());
    out.default_snapshot = std::move(def).value();
    for (const auto& [lang, snap] : by_lang) {
        auto c = compile_snapshot(snap, compressor);
        if (!c) return core::fail(c.error());
        out.by_lang.emplace(lang, std::move(c).value());
    }
    return out;
}

core::Result<pb::AnonGameInfoReply> build_inforeply_for_tag(
    std::uint32_t client_tag,
    std::uint32_t tag_unk,
    std::uint32_t count,
    const CompiledSnapshot& compiled,
    bool more) {
    auto looked = lookup_compiled(client_tag, compiled);
    if (!looked) return core::fail(looked.error());
    const auto* framed = looked.value();
    if (framed == nullptr) {
        return core::fail(core::make_error(
            core::StatusCode::NotFound,
            "anongame_inforeply: compiled snapshot has no payload for tag"));
    }
    auto stag = server_tag_for(client_tag);
    if (!stag) return core::fail(stag.error());

    pb::AnonGameInfoReply out{};
    out.count    = count;
    out.noitems  = 1;
    out.tag      = stag.value();
    out.tag_unk  = tag_unk;
    out.payload  = *framed;
    out.trailing = more ? std::uint8_t{0x01} : std::uint8_t{0x00};
    return out;
}

core::Result<std::vector<pb::AnonGameInfoReply>>
build_inforeplies_for_request(
    const pb::AnonGameInfoRequest& request,
    const CompiledSnapshot& compiled) {
    // First pass: figure out which requested tags have data, in order.
    std::vector<std::size_t> producing;
    producing.reserve(request.entries.size());
    for (std::size_t i = 0; i < request.entries.size(); ++i) {
        auto looked = lookup_compiled(request.entries[i].tag, compiled);
        if (!looked) return core::fail(looked.error());
        if (looked.value() != nullptr) producing.push_back(i);
    }

    std::vector<pb::AnonGameInfoReply> out;
    out.reserve(producing.size());
    for (std::size_t k = 0; k < producing.size(); ++k) {
        const auto& e    = request.entries[producing[k]];
        const bool  more = (k + 1 < producing.size());
        auto reply = build_inforeply_for_tag(
            e.tag, e.tag_unk, request.count, compiled, more);
        if (!reply) return core::fail(reply.error());
        out.push_back(std::move(reply).value());
    }
    return out;
}

core::Result<std::vector<std::byte>> encode_inforeplies_for_request(
    const pb::AnonGameInfoRequest& request,
    const CompiledSnapshot& compiled) {
    auto replies = build_inforeplies_for_request(request, compiled);
    if (!replies) return core::fail(replies.error());
    return encode_inforeply_packets(
        std::span<const pb::AnonGameInfoReply>{replies.value().data(),
                                                replies.value().size()});
}

}  // namespace pvpgn::application::anongame_inforeply
