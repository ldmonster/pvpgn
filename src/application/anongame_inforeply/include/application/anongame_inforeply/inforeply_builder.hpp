// SPDX-License-Identifier: GPL-2.0-or-later
//
// Service layer that composes a complete BNet FINDANONGAME INFOREPLY
// from typed payloads. Bridges three layers:
//
//   typed payload  (protocol::bnet::AnonGame{Url,Map,Type,Desc,Ladr}Payload)
//        |
//        v   serialize via protocol/bnet/anongame_tags
//   decompressed bytes
//        |
//        v   compress via infra/compression
//   framed bytes (4-byte header + deflate stream)
//        |
//        v   wrap in AnonGameInfoReply envelope
//   AnonGameInfoReply (one per requested tag)
//
// The client requests several tags in a single INFOREQ; the server
// answers with one INFOREPLY per tag, all sharing the same `count`
// and with the last reply marked `trailing == 0x00`. This module
// implements that fan-out.
//
// Reference: src/bnetd/handle_anongame.cpp::_client_findanongame_infos
//            and src/bnetd/anongame_infos.cpp.

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"

#include "domain/matchmaking/ports.hpp"
#include "protocol/bnet/anongame.hpp"
#include "protocol/bnet/anongame_tags.hpp"

namespace pvpgn::application::anongame_inforeply {

/// Server-side snapshot of the typed payloads that can be served in
/// reply to an INFOREQ. Each field is optional because realistic
/// deployments may not configure all five tag streams.
struct AnonGameInfoSnapshot {
    std::optional<protocol::bnet::AnonGameUrlPayload>  url;
    std::optional<protocol::bnet::AnonGameMapPayload>  map;
    std::optional<protocol::bnet::AnonGameTypePayload> type;
    std::optional<protocol::bnet::AnonGameDescPayload> desc;
    std::optional<protocol::bnet::AnonGameLadrPayload> ladr;
};

/// Pre-compiled (serialized + compressed + framed) form of an
/// `AnonGameInfoSnapshot`. Each tag is reduced to the exact bytes
/// that will ride in the wire `AnonGameInfoReply::payload` field —
/// no further serialization or compression is needed at request
/// time. This is the cache the legacy server keeps per
/// (war3/w3xp) × language.
struct CompiledSnapshot {
    std::optional<std::vector<std::uint8_t>> url;
    std::optional<std::vector<std::uint8_t>> map;
    std::optional<std::vector<std::uint8_t>> type;
    std::optional<std::vector<std::uint8_t>> desc;
    std::optional<std::vector<std::uint8_t>> ladr;
};

/// Serialize + compress every populated payload in `snapshot` into
/// the framed bytes the wire envelope will carry. Each tag's
/// compression is performed independently; if any one fails, the
/// whole compile fails and reports the error.
core::Result<CompiledSnapshot> compile_snapshot(
    const AnonGameInfoSnapshot& snapshot,
    const domain::matchmaking::IAnonGameCompressor& compressor);

/// Bundle of pre-compiled snapshots keyed by language. The legacy
/// server keeps one such bundle per (war3/w3xp) clienttag and uses
/// the requesting client's language to select which snapshot's
/// bytes to ship in the 0x44 reply. Locales not present in
/// `by_lang` fall back to `default_snapshot`.
struct CompiledSnapshotSet {
    CompiledSnapshot default_snapshot;
    std::unordered_map<std::string, CompiledSnapshot> by_lang;

    /// Return the compiled snapshot for `lang_id`, or
    /// `default_snapshot` when no locale-specific entry exists.
    const CompiledSnapshot& select(std::string_view lang_id) const;
};

/// Compile a multilocale set of typed snapshots into pre-compressed
/// per-locale bytes. Equivalent to calling `compile_snapshot` on the
/// default and on every locale-specific snapshot. Fails on the first
/// compile error.
core::Result<CompiledSnapshotSet> compile_snapshot_set(
    const AnonGameInfoSnapshot& default_snapshot,
    const std::unordered_map<std::string, AnonGameInfoSnapshot>& by_lang,
    const domain::matchmaking::IAnonGameCompressor& compressor);

/// Map a client-side request tag (e.g. `kAnonGameInfoTagURL` =
/// `'URL\0'`) to the matching server-reply tag (the byte-reversed
/// form, e.g. `kAnonGameInfoTagServerURL` = `'LRU\0'`). Returns
/// `InvalidArgument` for unknown tags.
core::Result<std::uint32_t> server_tag_for(std::uint32_t client_tag);

/// Low-level: build one INFOREPLY from already-serialized payload
/// bytes. Compresses `serialized_payload` via the supplied port and
/// stuffs the framed bytes into the envelope's `payload` field.
core::Result<protocol::bnet::AnonGameInfoReply> compose_inforeply(
    std::uint32_t server_tag,
    std::uint32_t tag_unk,
    std::uint32_t count,
    std::span<const std::uint8_t> serialized_payload,
    bool more,
    const domain::matchmaking::IAnonGameCompressor& compressor);

/// Build a single INFOREPLY for the given client-side tag, drawing
/// the typed payload from `snapshot`. Returns `NotFound` when the
/// snapshot has no payload for the requested tag.
core::Result<protocol::bnet::AnonGameInfoReply> build_inforeply_for_tag(
    std::uint32_t client_tag,
    std::uint32_t tag_unk,
    std::uint32_t count,
    const AnonGameInfoSnapshot& snapshot,
    bool more,
    const domain::matchmaking::IAnonGameCompressor& compressor);

/// Build the full INFOREPLY set for an entire INFOREQ. The `count`
/// from the request is propagated to every reply. The `trailing`
/// byte of the last reply is `0x00`; the others are `0x01`. Tags
/// missing from the snapshot are silently skipped (matches legacy
/// behaviour: server only emits replies for the tags it has data
/// for).
core::Result<std::vector<protocol::bnet::AnonGameInfoReply>>
build_inforeplies_for_request(
    const protocol::bnet::AnonGameInfoRequest& request,
    const AnonGameInfoSnapshot& snapshot,
    const domain::matchmaking::IAnonGameCompressor& compressor);

/// Serialize one `AnonGameInfoReply` to its full SID 0x44 packet
/// bytes (`0xFF, 0x44, len_lo, len_hi, sub_option=0x02, body...`).
core::Result<std::vector<std::byte>> encode_inforeply_packet(
    const protocol::bnet::AnonGameInfoReply& reply);

/// Serialize a sequence of `AnonGameInfoReply` envelopes into the
/// concatenated SID 0x44 byte stream that a real BNet session would
/// write back-to-back on the wire.
core::Result<std::vector<std::byte>> encode_inforeply_packets(
    std::span<const protocol::bnet::AnonGameInfoReply> replies);

/// Top-level helper: given the INFOREQ from the client and the
/// server's typed snapshot, produce the concatenated SID 0x44
/// byte stream of all INFOREPLY packets ready to write to the
/// socket. Equivalent to
/// `encode_inforeply_packets(build_inforeplies_for_request(...))`.
core::Result<std::vector<std::byte>> encode_inforeplies_for_request(
    const protocol::bnet::AnonGameInfoRequest& request,
    const AnonGameInfoSnapshot& snapshot,
    const domain::matchmaking::IAnonGameCompressor& compressor);

// =========================================================================
// Cached / pre-compiled overloads
// =========================================================================
//
// These mirror the typed-snapshot APIs but consume a `CompiledSnapshot`,
// avoiding the cost of re-serializing and re-compressing payloads on
// every request. Recommended for hot paths.

/// Build a single INFOREPLY for the given client-side tag, using the
/// pre-compressed framed bytes from the compiled snapshot.
core::Result<protocol::bnet::AnonGameInfoReply> build_inforeply_for_tag(
    std::uint32_t client_tag,
    std::uint32_t tag_unk,
    std::uint32_t count,
    const CompiledSnapshot& compiled,
    bool more);

core::Result<std::vector<protocol::bnet::AnonGameInfoReply>>
build_inforeplies_for_request(
    const protocol::bnet::AnonGameInfoRequest& request,
    const CompiledSnapshot& compiled);

core::Result<std::vector<std::byte>> encode_inforeplies_for_request(
    const protocol::bnet::AnonGameInfoRequest& request,
    const CompiledSnapshot& compiled);

}  // namespace pvpgn::application::anongame_inforeply
