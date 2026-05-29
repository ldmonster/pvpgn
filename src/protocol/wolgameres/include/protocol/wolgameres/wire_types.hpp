// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file wire_types.hpp
/// Westwood Online "wol_gameres" wire types, mirrored from
/// `src/common/wol_gameres_protocol.h`.
///
/// The protocol uses tag/length/value records keyed by 4-byte ASCII
/// `Tag` values (e.g. `'PNAM'`). Many tags are indexed 0..7 (one
/// per game slot); we expose those as compile-time arrays.

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace pvpgn::protocol::wolgameres::wire {

/// 4-byte ASCII tag, transmitted as an unsigned 32-bit value.
using Tag = std::uint32_t;

/// Build a tag from 4 ASCII characters (most-significant byte first).
constexpr Tag make_tag(char a, char b, char c, char d) noexcept
{
    return (static_cast<Tag>(static_cast<std::uint8_t>(a)) << 24)
         | (static_cast<Tag>(static_cast<std::uint8_t>(b)) << 16)
         | (static_cast<Tag>(static_cast<std::uint8_t>(c)) << 8)
         | (static_cast<Tag>(static_cast<std::uint8_t>(d)));
}

/// Build an indexed tag, where the last char is `'0' + n`.
constexpr Tag make_indexed_tag(char a, char b, char c, std::uint8_t n) noexcept
{
    return make_tag(a, b, c, static_cast<char>('0' + n));
}

/// Build an array of 8 indexed tags `Tag{a,b,c,'0'..'7'}`.
constexpr std::array<Tag, 8> indexed_tag_set(char a, char b, char c) noexcept
{
    std::array<Tag, 8> out{};
    for (std::uint8_t i = 0; i < 8; ++i)
        out[i] = make_indexed_tag(a, b, c, i);
    return out;
}

/// Per-record header: total record size + RNDG sub-block size.
struct Header
{
    std::uint16_t size      = 0;
    std::uint16_t rngd_size = 0;
    constexpr bool operator==(const Header&) const = default;
};
static_assert(sizeof(Header) == 4);
static_assert(std::is_trivially_copyable_v<Header>);

// ---- Data type codes ---------------------------------------------------

inline constexpr int kDataTypeByte   = 1;
inline constexpr int kDataTypeBool   = 2;
inline constexpr int kDataTypeTime   = 5;
inline constexpr int kDataTypeInt    = 6;
inline constexpr int kDataTypeString = 7;
inline constexpr int kDataTypeBigint = 20;

// ---- Game resolution (top-level) tags ---------------------------------

inline constexpr Tag kClientSern = 0x53455223;
inline constexpr Tag kClientSdfx = 0x53444658;
inline constexpr Tag kClientIdno = 0x49444E4F;
inline constexpr Tag kClientGsku = 0x47534B55;
inline constexpr Tag kClientDcon = 0x44434F4E;
inline constexpr Tag kClientLcon = 0x4C434F4E;
inline constexpr Tag kClientType = 0x54595045;
inline constexpr Tag kClientTrny = 0x54524E59;
inline constexpr Tag kClientOosy = 0x4F4F5359;
inline constexpr Tag kClientFini = 0x46494E49;
inline constexpr Tag kClientDura = 0x44555241;
inline constexpr Tag kClientCred = 0x43524544;
inline constexpr Tag kClientShrt = 0x53485254;
inline constexpr Tag kClientSupr = 0x53555052;
inline constexpr Tag kClientMode = 0x4D4F4445;
inline constexpr Tag kClientBamr = 0x42414D52;
inline constexpr Tag kClientCrat = 0x43524154;
inline constexpr Tag kClientAipl = 0x4149504C;
inline constexpr Tag kClientUnit = 0x554E4954;
inline constexpr Tag kClientScen = 0x5343454E;
inline constexpr Tag kClientCmpl = 0x434D504C;
inline constexpr Tag kClientPngs = 0x504E4753;
inline constexpr Tag kClientPngr = 0x504E4752;
inline constexpr Tag kClientPlrs = 0x504C5253;
inline constexpr Tag kClientSpid = 0x53504944;
inline constexpr Tag kClientTime = 0x54494D45;
inline constexpr Tag kClientAfps = 0x41465053;
inline constexpr Tag kClientProc = 0x50524F43;
inline constexpr Tag kClientMemo = 0x4D454D4F;
inline constexpr Tag kClientVidm = 0x5649444D;
inline constexpr Tag kClientSped = 0x53504544;
inline constexpr Tag kClientVers = 0x56455253;
inline constexpr Tag kClientDate = 0x44415445;
inline constexpr Tag kClientBase = 0x42415345;
inline constexpr Tag kClientTibr = 0x54494252;
inline constexpr Tag kClientShad = 0x53484144;
inline constexpr Tag kClientFlag = 0x464C4147;
inline constexpr Tag kClientTech = 0x54454348;
inline constexpr Tag kClientBrok = 0x42524f4b;
inline constexpr Tag kClientAcco = 0x4143434f;
inline constexpr Tag kClientEtim = 0x4554494d;
inline constexpr Tag kClientPspd = 0x50535044;
inline constexpr Tag kClientSmem = 0x534d454d;
inline constexpr Tag kClientSvid = 0x53564944;
inline constexpr Tag kClientSnam = 0x534e414d;
inline constexpr Tag kClientGmap = 0x474d4150;
inline constexpr Tag kClientDsvr = 0x44535652;

// ---- RNDG (per-player) sub-block tags ---------------------------------

inline constexpr Tag kClientPnam = 0x504e414d;
inline constexpr Tag kClientPloc = 0x504c4f43;
inline constexpr Tag kClientTeam = 0x5445414d;
inline constexpr Tag kClientPscr = 0x50534352;
inline constexpr Tag kClientPpts = 0x50505453;
inline constexpr Tag kClientPtim = 0x5054494d;
inline constexpr Tag kClientPhlt = 0x50484c54;
inline constexpr Tag kClientPkil = 0x504b494c;
inline constexpr Tag kClientEkil = 0x454b494c;
inline constexpr Tag kClientAkil = 0x414b494c;
inline constexpr Tag kClientShot = 0x53484f54;
inline constexpr Tag kClientHedf = 0x48454446;
inline constexpr Tag kClientTorf = 0x544f5246;
inline constexpr Tag kClientArmf = 0x41524d46;
inline constexpr Tag kClientLegf = 0x4c454746;
inline constexpr Tag kClientCrtf = 0x43525446;
inline constexpr Tag kClientPups = 0x50555053;
inline constexpr Tag kClientVkil = 0x564b494c;
inline constexpr Tag kClientVtim = 0x5654494d;
inline constexpr Tag kClientNkfv = 0x4e4b4656;
inline constexpr Tag kClientSqui = 0x53515549;
inline constexpr Tag kClientPcrd = 0x50435244;
inline constexpr Tag kClientBkil = 0x424b494c;
inline constexpr Tag kClientHedr = 0x48454452;
inline constexpr Tag kClientTorr = 0x544f5252;
inline constexpr Tag kClientArmr = 0x41524d52;
inline constexpr Tag kClientLegr = 0x4c454752;
inline constexpr Tag kClientCrtr = 0x43525452;
inline constexpr Tag kClientFlgc = 0x464c4743;

// ---- Indexed per-slot player tags (suffix 0..7) -----------------------
//
// `kClientNam[3] == 0x4E414D33  /* 'NAM3' */`, etc.

inline constexpr auto kClientNam = indexed_tag_set('N', 'A', 'M');
inline constexpr auto kClientIpa = indexed_tag_set('I', 'P', 'A');
inline constexpr auto kClientCid = indexed_tag_set('C', 'I', 'D');
inline constexpr auto kClientSid = indexed_tag_set('S', 'I', 'D');
inline constexpr auto kClientTid = indexed_tag_set('T', 'I', 'D');
inline constexpr auto kClientCmp = indexed_tag_set('C', 'M', 'P');
inline constexpr auto kClientCol = indexed_tag_set('C', 'O', 'L');
inline constexpr auto kClientCrd = indexed_tag_set('C', 'R', 'D');
inline constexpr auto kClientInb = indexed_tag_set('I', 'N', 'B');
inline constexpr auto kClientUnb = indexed_tag_set('U', 'N', 'B');
inline constexpr auto kClientPlb = indexed_tag_set('P', 'L', 'B');
inline constexpr auto kClientBlb = indexed_tag_set('B', 'L', 'B');
inline constexpr auto kClientInl = indexed_tag_set('I', 'N', 'L');
inline constexpr auto kClientUnl = indexed_tag_set('U', 'N', 'L');
inline constexpr auto kClientPll = indexed_tag_set('P', 'L', 'L');
inline constexpr auto kClientBll = indexed_tag_set('B', 'L', 'L');
inline constexpr auto kClientInk = indexed_tag_set('I', 'N', 'K');
inline constexpr auto kClientUnk = indexed_tag_set('U', 'N', 'K');
inline constexpr auto kClientPlk = indexed_tag_set('P', 'L', 'K');
inline constexpr auto kClientBlk = indexed_tag_set('B', 'L', 'K');
inline constexpr auto kClientBlc = indexed_tag_set('B', 'L', 'C');

}  // namespace pvpgn::protocol::wolgameres::wire
