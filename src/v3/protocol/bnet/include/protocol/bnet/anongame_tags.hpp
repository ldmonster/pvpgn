// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file anongame_tags.hpp
/// Typed payload parsers/serializers for the URL / MAP / TYPE / DESC / LADR
/// tag streams carried inside `AnonGameInfoReply::payload`.
///
/// IMPORTANT: on the wire these payloads are **zlib-compressed** by the
/// legacy server (see `bnetd/anongame_infos.cpp::anongame_infos_data_load`
/// — `zlib_compress(...)` calls for each comp_data). The functions in this
/// header operate on the **decompressed** bytes only. Callers are
/// responsible for plugging in their own compression adapter on the
/// transport side; v3 does not pull zlib into the protocol layer.
///
/// Layouts taken verbatim from `bnetd/anongame_infos.cpp`,
/// `bnetd/anongame_maplists.cpp` and `bnetd/handle_anongame.cpp`.

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "core/result.hpp"

namespace pvpgn::protocol::bnet {

// --- URL -----------------------------------------------------------------
// Pre-1.15: 3 NUL-terminated strings (server URL, player URL, tourney URL).
// >=1.15:   4 strings (above + clan URL).
struct AnonGameUrlPayload {
    std::vector<std::string> urls;
    bool operator==(const AnonGameUrlPayload&) const = default;
};

// --- MAP -----------------------------------------------------------------
// 1-byte count prefix + `count` NUL-terminated mapname strings.
struct AnonGameMapPayload {
    std::vector<std::string> mapnames;  // count fits in u8
    bool operator==(const AnonGameMapPayload&) const = default;
};

// --- TYPE ----------------------------------------------------------------
// 1-byte section count + per section:
//   1-byte section_id (0=PG, 1=TY, 2=AT) +
//   1-byte gamestyle_count + per gamestyle:
//     5-byte queue prefix +
//     map_info: 1-byte map_count + `map_count` map-index bytes.
struct AnonGameTypeGamestyle {
    std::array<std::uint8_t, 5> prefix{};
    std::vector<std::uint8_t>   map_indices;  // length fits in u8
    bool operator==(const AnonGameTypeGamestyle&) const = default;
};
struct AnonGameTypeSection {
    std::uint8_t                       section_id = 0;
    std::vector<AnonGameTypeGamestyle> gamestyles;  // length fits in u8
    bool operator==(const AnonGameTypeSection&) const = default;
};
struct AnonGameTypePayload {
    std::vector<AnonGameTypeSection> sections;  // length fits in u8
    bool operator==(const AnonGameTypePayload&) const = default;
};

// --- DESC ----------------------------------------------------------------
// 1-byte total_count + per entry:
//   1-byte section_id + 1-byte gametype_id + short_desc (cstr) + long_desc (cstr).
struct AnonGameDescEntry {
    std::uint8_t section_id  = 0;
    std::uint8_t gametype_id = 0;
    std::string  short_desc;
    std::string  long_desc;
    bool operator==(const AnonGameDescEntry&) const = default;
};
struct AnonGameDescPayload {
    std::vector<AnonGameDescEntry> entries;  // length fits in u8
    bool operator==(const AnonGameDescPayload&) const = default;
};

// --- LADR ----------------------------------------------------------------
// 1-byte count + per entry:
//   4-byte tag (e.g. 'OLOS','MAET',' AFF','2SV2','3SV3','4SV4','SNLC','2NLC',...) +
//   desc (cstr) + url (cstr).
struct AnonGameLadrEntry {
    std::uint32_t tag = 0;
    std::string   desc;
    std::string   url;
    bool operator==(const AnonGameLadrEntry&) const = default;
};
struct AnonGameLadrPayload {
    std::vector<AnonGameLadrEntry> entries;  // legacy emits 10 entries
    bool operator==(const AnonGameLadrPayload&) const = default;
};

// --- parse / serialize ---------------------------------------------------
// All parsers consume the entire input buffer (extra trailing bytes are
// reported as `OutOfRange`). All serializers emit the canonical layout.

core::Result<AnonGameUrlPayload>  parse_url_payload (const std::vector<std::uint8_t>& bytes,
                                                     std::uint8_t expected_count = 0);
core::Result<AnonGameMapPayload>  parse_map_payload (const std::vector<std::uint8_t>& bytes);
core::Result<AnonGameTypePayload> parse_type_payload(const std::vector<std::uint8_t>& bytes);
core::Result<AnonGameDescPayload> parse_desc_payload(const std::vector<std::uint8_t>& bytes);
core::Result<AnonGameLadrPayload> parse_ladr_payload(const std::vector<std::uint8_t>& bytes);

std::vector<std::uint8_t> serialize_url_payload (const AnonGameUrlPayload&);
std::vector<std::uint8_t> serialize_map_payload (const AnonGameMapPayload&);
std::vector<std::uint8_t> serialize_type_payload(const AnonGameTypePayload&);
std::vector<std::uint8_t> serialize_desc_payload(const AnonGameDescPayload&);
std::vector<std::uint8_t> serialize_ladr_payload(const AnonGameLadrPayload&);

}  // namespace pvpgn::protocol::bnet
