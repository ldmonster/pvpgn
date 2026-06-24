// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file clan_rank_wire.hpp
/// Translation between the `ClanRank` *domain* enum and the legacy Battle.net
/// *wire* rank byte (and the byte persisted in storage, which is the same
/// legacy value).
///
/// The two numbering schemes are deliberately **inverted**:
///
///   * The domain enum (`clan.hpp`) numbers ranks so that a *lower* value means
///     *higher* authority (`Chieftain = 1 … Peon = 4`). All in-aggregate
///     authority checks (`rank > ClanRank::Shaman` == "outranked by Shaman")
///     depend on that ordering, so the enum values must not be renumbered.
///
///   * The legacy wire / DB byte numbers ranks so that a *higher* value means
///     *higher* authority (`Peon = 0x01 … Chieftain = 0x04`), matching
///     `src/bnetd/clan.h` (`CLAN_CHIEFTAIN 0x04 … CLAN_PEON 0x01`) and the
///     `kRank*` constants in `protocol/bnet/clan_wire_types.hpp`.
///
/// Every boundary that emits or stores a rank (wire serialization, persistence)
/// MUST translate through these helpers; serializing the raw enum value would
/// transmit / store a Chieftain as wire-Peon and vice-versa.

#include <cstdint>

#include "domain/social/clan.hpp"

namespace pvpgn::domain::social {

/// Legacy clan-rank byte values (mirror of `src/bnetd/clan.h` and of the
/// `kRank*` constants in `protocol/bnet/clan_wire_types.hpp`). Duplicated here
/// as plain literals so the domain layer carries no dependency on the protocol
/// layer; the values are pinned by the unit tests.
inline constexpr std::uint8_t kWireRankNew       = 0x00;
inline constexpr std::uint8_t kWireRankPeon      = 0x01;
inline constexpr std::uint8_t kWireRankGrunt     = 0x02;
inline constexpr std::uint8_t kWireRankShaman    = 0x03;
inline constexpr std::uint8_t kWireRankChieftain = 0x04;

/// Map a domain `ClanRank` to its legacy wire / storage byte.
[[nodiscard]] inline constexpr std::uint8_t clan_rank_to_wire(ClanRank r) noexcept {
    switch (r) {
        case ClanRank::Chieftain: return kWireRankChieftain;  // 0x04
        case ClanRank::Shaman:    return kWireRankShaman;     // 0x03
        case ClanRank::Grunt:     return kWireRankGrunt;      // 0x02
        case ClanRank::Peon:      return kWireRankPeon;       // 0x01
    }
    // Unknown/out-of-range domain value: fall back to the lowest rank rather
    // than emitting an undefined byte.
    return kWireRankPeon;
}

/// Map a legacy wire / storage byte to a domain `ClanRank`. The legacy
/// `CLAN_NEW` (0x00) probation rank is not modeled in the domain; it (and any
/// unrecognized byte) maps to the lowest modeled rank, `Peon`.
[[nodiscard]] inline constexpr ClanRank clan_rank_from_wire(std::uint8_t b) noexcept {
    switch (b) {
        case kWireRankChieftain: return ClanRank::Chieftain;  // 0x04
        case kWireRankShaman:    return ClanRank::Shaman;      // 0x03
        case kWireRankGrunt:     return ClanRank::Grunt;       // 0x02
        case kWireRankPeon:      return ClanRank::Peon;        // 0x01
        case kWireRankNew:       return ClanRank::Peon;        // 0x00 (NEW->Peon)
        default:                 return ClanRank::Peon;
    }
}

}  // namespace pvpgn::domain::social
