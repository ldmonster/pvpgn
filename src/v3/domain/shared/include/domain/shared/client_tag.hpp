// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file client_tag.hpp
/// `ClientTag` — 4-byte product/architecture/language code shared by every
/// BNet message (`STAR`, `D2DV`, `D2XP`, `WAR3`, `W3XP`, …).
///
/// Stored as an `std::array<char,4>` in *human-readable* order so
/// `ClientTag{"STAR"}.text() == "STAR"`. Wire encoding (legacy is
/// reversed bytes — `RATS`) belongs to the protocol layer.
///
/// Known tag constants live in the `pvpgn::domain::tags` namespace:
///   `tags::kStarcraft`, `tags::kBroodWar`, `tags::kWarcraft3`, …
///
/// Migration note: replaces `src/common/tag.h` `t_clienttag` / `t_archtag` /
/// `t_gamelang` (all `uint32_t` aliases) and the `CLIENTTAG_*_UINT` macros.

#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string_view>

#include "core/error.hpp"
#include "core/result.hpp"

namespace pvpgn::domain {

// ---------------------------------------------------------------------------
// ClientTag
// ---------------------------------------------------------------------------

class ClientTag {
public:
    /// Construct from a 4-character ASCII tag. Validates that all four
    /// bytes are printable ASCII (0x20–0x7E). Returns `InvalidArgument`
    /// otherwise.
    static core::Result<ClientTag> parse(std::string_view s) {
        if (s.size() != 4) {
            return core::fail(core::Error{
                core::StatusCode::InvalidArgument,
                "ClientTag must be exactly 4 characters"});
        }
        for (char c : s) {
            const auto u = static_cast<unsigned char>(c);
            if (u < 0x20 || u > 0x7E) {
                return core::fail(core::Error{
                    core::StatusCode::InvalidArgument,
                    "ClientTag contains non-printable byte"});
            }
        }
        ClientTag t;
        std::memcpy(t.v_.data(), s.data(), 4);
        return t;
    }

    /// Construct from a big-endian packed uint32 (e.g. `0x53544152` → `STAR`).
    /// Returns `InvalidArgument` if any byte is non-printable.
    static core::Result<ClientTag> from_packed_be(std::uint32_t v) {
        char buf[4];
        buf[0] = static_cast<char>((v >> 24) & 0xFF);
        buf[1] = static_cast<char>((v >> 16) & 0xFF);
        buf[2] = static_cast<char>((v >>  8) & 0xFF);
        buf[3] = static_cast<char>( v        & 0xFF);
        return parse(std::string_view{buf, 4});
    }

    constexpr ClientTag() = default;

    /// Unchecked constructor — for compile-time literals. Caller
    /// guarantees the source is 4 printable ASCII bytes.
    explicit constexpr ClientTag(std::array<char, 4> v) : v_(v) {}

    constexpr const std::array<char, 4>& bytes() const noexcept { return v_; }

    constexpr std::string_view text() const noexcept {
        return std::string_view{v_.data(), v_.size()};
    }

    /// Returns the tag packed as a big-endian uint32 (same as the legacy
    /// `CLIENTTAG_*_UINT` macros, e.g. `STAR` → `0x53544152`).
    constexpr std::uint32_t packed_be() const noexcept {
        return (static_cast<std::uint32_t>(static_cast<unsigned char>(v_[0])) << 24)
             | (static_cast<std::uint32_t>(static_cast<unsigned char>(v_[1])) << 16)
             | (static_cast<std::uint32_t>(static_cast<unsigned char>(v_[2])) <<  8)
             |  static_cast<std::uint32_t>(static_cast<unsigned char>(v_[3]));
    }

    /// Returns true if this tag is in the known-valid client list
    /// (mirrors legacy `tag_check_client()`).
    constexpr bool is_valid_client() const noexcept;

    /// Returns true if this tag is a known architecture tag
    /// (mirrors legacy `tag_check_arch()`).
    constexpr bool is_valid_arch() const noexcept;

    /// Returns true if this tag is a Westwood Online v1 client
    /// (mirrors legacy `tag_check_wolv1()`).
    constexpr bool is_wol_v1() const noexcept;

    /// Returns true if this tag is a Westwood Online v2 client
    /// (mirrors legacy `tag_check_wolv2()`).
    constexpr bool is_wol_v2() const noexcept;

    /// Returns a human-readable product title (mirrors legacy
    /// `clienttag_get_title()`). Returns "Unknown" for unrecognised tags.
    constexpr std::string_view title() const noexcept;

    constexpr auto operator<=>(const ClientTag&) const = default;

private:
    std::array<char, 4> v_{{' ', ' ', ' ', ' '}};
};

// ---------------------------------------------------------------------------
// Known tag constants
// ---------------------------------------------------------------------------

namespace tags {

// ---- Blizzard client tags -------------------------------------------------

/// Chat bot / BNCS chat client
inline constexpr ClientTag kBnChatBot  {{'C','H','A','T'}};  // 0x43484154
/// StarCraft (original)
inline constexpr ClientTag kStarcraft  {{'S','T','A','R'}};  // 0x53544152
/// StarCraft: Brood War
inline constexpr ClientTag kBroodWar   {{'S','E','X','P'}};  // 0x53455850
/// StarCraft Shareware
inline constexpr ClientTag kShareware  {{'S','S','H','R'}};  // 0x53534852
/// Diablo I (retail)
inline constexpr ClientTag kDiabloRtl  {{'D','R','T','L'}};  // 0x4452544C
/// Diablo I (shareware)
inline constexpr ClientTag kDiabloShr  {{'D','S','H','R'}};  // 0x44534852
/// WarCraft II Battle.net Edition
inline constexpr ClientTag kWarcraftII {{'W','2','B','N'}};  // 0x5732424E
/// Diablo II
inline constexpr ClientTag kDiablo2    {{'D','2','D','V'}};  // 0x44324456
/// StarCraft (Japan)
inline constexpr ClientTag kStarcraftJp{{'J','S','T','R'}};  // 0x4A535452
/// Diablo II Stress Test
inline constexpr ClientTag kDiablo2St  {{'D','2','S','T'}};  // 0x44325354
/// Diablo II: Lord of Destruction
inline constexpr ClientTag kDiablo2Xp  {{'D','2','X','P'}};  // 0x44325850
/// WarCraft III: Reign of Chaos
inline constexpr ClientTag kWarcraft3  {{'W','A','R','3'}};  // 0x57415233
/// WarCraft III: The Frozen Throne
inline constexpr ClientTag kWar3Xp     {{'W','3','X','P'}};  // 0x57335850
/// IRC client
inline constexpr ClientTag kIIrc       {{'I','I','R','C'}};  // 0x49495243

// ---- Westwood Online client tags -----------------------------------------

/// Westwood Chat
inline constexpr ClientTag kWChat      {{'W','C','H','T'}};  // 0x57434854
/// Tiberian Sun
inline constexpr ClientTag kTiberianSun{{'T','S','U','N'}};  // 0x5453554E
/// Tiberian Sun: Firestorm
inline constexpr ClientTag kTibSunXp   {{'T','S','X','P'}};  // 0x54535850
/// Red Alert 1
inline constexpr ClientTag kRedAlert   {{'R','A','L','T'}};  // 0x52414C54
/// Red Alert 2
inline constexpr ClientTag kRedAlert2  {{'R','A','L','2'}};  // 0x52414C32
/// Dune 2000
inline constexpr ClientTag kDune2000   {{'D','N','2','K'}};  // 0x444E324B
/// Nox
inline constexpr ClientTag kNox        {{'N','O','X','X'}};  // 0x4E4F5858
/// Nox Quest
inline constexpr ClientTag kNoxQuest   {{'N','O','X','Q'}};  // 0x4E4F5851
/// C&C Renegade
inline constexpr ClientTag kRenegade   {{'R','N','G','D'}};  // 0x524E4744
/// C&C Renegade Free Dedicated Server
inline constexpr ClientTag kRenegadeFds{{'R','F','D','S'}};  // 0x52464453
/// Yuri's Revenge
inline constexpr ClientTag kYurisRev   {{'Y','U','R','I'}};  // 0x59555249
/// Emperor: Battle for Dune
inline constexpr ClientTag kEmperorBd  {{'E','B','F','D'}};  // 0x45424644
/// Lands of Lore 3
inline constexpr ClientTag kLofLore3   {{'L','O','R','3'}};  // 0x4C4F5233
/// Generic Westwood Online
inline constexpr ClientTag kWwol       {{'W','W','O','L'}};  // 0x57574F4C

// ---- Sentinel / fallback -------------------------------------------------

/// Unknown / unrecognised tag
inline constexpr ClientTag kUnknown    {{'U','N','K','N'}};  // 0x554E4B4E

// ---- Architecture tags ---------------------------------------------------

/// MS Windows on Intel x86
inline constexpr ClientTag kArchWinX86 {{'I','X','8','6'}};  // 0x49583836
/// MacOS on PowerPC
inline constexpr ClientTag kArchMacPpc {{'P','M','A','C'}};  // 0x504D4143
/// MacOS X on PowerPC
inline constexpr ClientTag kArchOsxPpc {{'X','M','A','C'}};  // 0x584D4143

// ---- Game language tags --------------------------------------------------

/// English (US)
inline constexpr ClientTag kLangEnUs   {{'e','n','U','S'}};  // 0x656E5553
/// German
inline constexpr ClientTag kLangDeDE   {{'d','e','D','E'}};  // 0x64654445
/// Czech
inline constexpr ClientTag kLangCsCZ   {{'c','s','C','Z'}};  // 0x6373435A
/// Spanish
inline constexpr ClientTag kLangEsES   {{'e','s','E','S'}};  // 0x65734553
/// French
inline constexpr ClientTag kLangFrFR   {{'f','r','F','R'}};  // 0x66724652
/// Italian
inline constexpr ClientTag kLangItIT   {{'i','t','I','T'}};  // 0x69744954
/// Japanese
inline constexpr ClientTag kLangJaJA   {{'j','a','J','A'}};  // 0x6A614A41
/// Korean
inline constexpr ClientTag kLangKoKR   {{'k','o','K','R'}};  // 0x6B6F4B52
/// Polish
inline constexpr ClientTag kLangPlPL   {{'p','l','P','L'}};  // 0x706C504C
/// Russian
inline constexpr ClientTag kLangRuRU   {{'r','u','R','U'}};  // 0x72755255
/// Chinese Simplified
inline constexpr ClientTag kLangZhCN   {{'z','h','C','N'}};  // 0x7A68434E
/// Chinese Traditional
inline constexpr ClientTag kLangZhTW   {{'z','h','T','W'}};  // 0x7A685457

}  // namespace tags

// ---------------------------------------------------------------------------
// ClientTag method implementations (inline, constexpr where possible)
// ---------------------------------------------------------------------------

constexpr bool ClientTag::is_valid_client() const noexcept {
    const auto be = packed_be();
    return be == tags::kBnChatBot.packed_be()
        || be == tags::kStarcraft.packed_be()
        || be == tags::kBroodWar.packed_be()
        || be == tags::kShareware.packed_be()
        || be == tags::kDiabloRtl.packed_be()
        || be == tags::kDiabloShr.packed_be()
        || be == tags::kWarcraftII.packed_be()
        || be == tags::kDiablo2.packed_be()
        || be == tags::kStarcraftJp.packed_be()
        || be == tags::kDiablo2St.packed_be()
        || be == tags::kDiablo2Xp.packed_be()
        || be == tags::kWarcraft3.packed_be()
        || be == tags::kWar3Xp.packed_be()
        || be == tags::kIIrc.packed_be()
        || be == tags::kWChat.packed_be()
        || be == tags::kTiberianSun.packed_be()
        || be == tags::kTibSunXp.packed_be()
        || be == tags::kRedAlert.packed_be()
        || be == tags::kRedAlert2.packed_be()
        || be == tags::kDune2000.packed_be()
        || be == tags::kNox.packed_be()
        || be == tags::kNoxQuest.packed_be()
        || be == tags::kRenegade.packed_be()
        || be == tags::kRenegadeFds.packed_be()
        || be == tags::kYurisRev.packed_be()
        || be == tags::kEmperorBd.packed_be()
        || be == tags::kLofLore3.packed_be()
        || be == tags::kWwol.packed_be();
}

constexpr bool ClientTag::is_valid_arch() const noexcept {
    const auto be = packed_be();
    return be == tags::kArchWinX86.packed_be()
        || be == tags::kArchMacPpc.packed_be()
        || be == tags::kArchOsxPpc.packed_be();
}

constexpr bool ClientTag::is_wol_v1() const noexcept {
    return packed_be() == tags::kWChat.packed_be();
}

constexpr bool ClientTag::is_wol_v2() const noexcept {
    const auto be = packed_be();
    return be == tags::kTiberianSun.packed_be()
        || be == tags::kTibSunXp.packed_be()
        || be == tags::kRedAlert.packed_be()
        || be == tags::kRedAlert2.packed_be()
        || be == tags::kDune2000.packed_be()
        || be == tags::kNox.packed_be()
        || be == tags::kNoxQuest.packed_be()
        || be == tags::kRenegade.packed_be()
        || be == tags::kRenegadeFds.packed_be()
        || be == tags::kYurisRev.packed_be()
        || be == tags::kEmperorBd.packed_be()
        || be == tags::kLofLore3.packed_be()
        || be == tags::kWwol.packed_be();
}

constexpr std::string_view ClientTag::title() const noexcept {
    const auto be = packed_be();
    if (be == tags::kWar3Xp.packed_be())       return "Warcraft III: The Frozen Throne";
    if (be == tags::kWarcraft3.packed_be())     return "Warcraft III";
    if (be == tags::kDiablo2Xp.packed_be())     return "Diablo II: Lord of Destruction";
    if (be == tags::kDiablo2.packed_be())        return "Diablo II";
    if (be == tags::kStarcraftJp.packed_be())   return "Starcraft (Japan)";
    if (be == tags::kWarcraftII.packed_be())    return "Warcraft II";
    if (be == tags::kDiabloShr.packed_be())     return "Diablo I (Shareware)";
    if (be == tags::kDiabloRtl.packed_be())     return "Diablo I";
    if (be == tags::kShareware.packed_be())     return "Starcraft (Shareware)";
    if (be == tags::kBroodWar.packed_be())      return "Starcraft: Brood War";
    if (be == tags::kStarcraft.packed_be())     return "Starcraft";
    if (be == tags::kBnChatBot.packed_be())     return "Chat";
    if (be == tags::kIIrc.packed_be())          return "Internet Relay Chat";
    if (be == tags::kWChat.packed_be())         return "Westwood Chat";
    if (be == tags::kTiberianSun.packed_be())   return "Tiberian Sun";
    if (be == tags::kTibSunXp.packed_be())      return "Tiberian Sun: Firestorm";
    if (be == tags::kRedAlert.packed_be())      return "Red Alert";
    if (be == tags::kRedAlert2.packed_be())     return "Red Alert 2";
    if (be == tags::kDune2000.packed_be())      return "Dune 2000";
    if (be == tags::kNox.packed_be())           return "Nox";
    if (be == tags::kNoxQuest.packed_be())      return "Nox Quest";
    if (be == tags::kRenegade.packed_be())      return "Renegade";
    if (be == tags::kRenegadeFds.packed_be())   return "Renegade Free Dedicated Server";
    if (be == tags::kYurisRev.packed_be())      return "Yuri's Revenge";
    if (be == tags::kEmperorBd.packed_be())     return "Emperor: Battle for Dune";
    if (be == tags::kLofLore3.packed_be())      return "Lands of Lore 3";
    if (be == tags::kWwol.packed_be())          return "Westwood Online";
    return "Unknown";
}

}  // namespace pvpgn::domain

// ---------------------------------------------------------------------------
// std::hash specialisation
// ---------------------------------------------------------------------------

namespace std {
template <>
struct hash<pvpgn::domain::ClientTag> {
    std::size_t operator()(const pvpgn::domain::ClientTag& t) const noexcept {
        return std::hash<std::uint32_t>{}(t.packed_be());
    }
};
}  // namespace std
