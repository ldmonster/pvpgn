// SPDX-License-Identifier: GPL-2.0-or-later
/// @file client_tag_test.cpp
/// Catch2 unit tests for `pvpgn::domain::ClientTag` and the `tags::k*`
/// constants defined in `domain/shared/client_tag.hpp`.
///
/// Coverage:
///   - Construction: parse(), from_packed_be(), constexpr array ctor
///   - Accessors: text(), bytes(), packed_be()
///   - Validation: parse() rejects wrong length / non-printable bytes
///   - from_packed_be() round-trips with packed_be()
///   - Known constants: packed_be() values match legacy CLIENTTAG_*_UINT macros
///   - is_valid_client() / is_valid_arch() / is_wol_v1() / is_wol_v2()
///   - title() returns correct human-readable strings
///   - Comparison operators (==, !=, <=>)
///   - std::hash specialisation

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <unordered_set>

#include "domain/shared/client_tag.hpp"

using namespace pvpgn;
using namespace pvpgn::domain;
using namespace pvpgn::domain::tags;

// ---------------------------------------------------------------------------
// Construction & accessors
// ---------------------------------------------------------------------------

TEST_CASE("ClientTag: parse() accepts 4-byte printable ASCII", "[domain][shared][client_tag]") {
    auto t = ClientTag::parse("STAR");
    REQUIRE(t.has_value());
    REQUIRE(std::string{t.value().text()} == "STAR");
    REQUIRE(t.value().packed_be() == 0x53544152u);
}

TEST_CASE("ClientTag: parse() rejects wrong length", "[domain][shared][client_tag]") {
    REQUIRE_FALSE(ClientTag::parse("").has_value());
    REQUIRE_FALSE(ClientTag::parse("STA").has_value());
    REQUIRE_FALSE(ClientTag::parse("STARS").has_value());
    REQUIRE_FALSE(ClientTag::parse("STARCRAFT").has_value());
}

TEST_CASE("ClientTag: parse() rejects non-printable bytes", "[domain][shared][client_tag]") {
    REQUIRE_FALSE(ClientTag::parse(std::string_view{"ST\x01R", 4}).has_value());
    REQUIRE_FALSE(ClientTag::parse(std::string_view{"\x00TAR", 4}).has_value());
    REQUIRE_FALSE(ClientTag::parse(std::string_view{"STA\x7F", 4}).has_value());
}

TEST_CASE("ClientTag: parse() accepts full printable range boundaries", "[domain][shared][client_tag]") {
    // 0x20 (space) and 0x7E (~) are the boundary printable chars
    REQUIRE(ClientTag::parse(std::string_view{" TAR", 4}).has_value());
    REQUIRE(ClientTag::parse(std::string_view{"STA~", 4}).has_value());
}

TEST_CASE("ClientTag: from_packed_be() round-trips with packed_be()", "[domain][shared][client_tag]") {
    const std::uint32_t val = 0x53544152u;  // STAR
    auto t = ClientTag::from_packed_be(val);
    REQUIRE(t.has_value());
    REQUIRE(t.value().packed_be() == val);
    REQUIRE(std::string{t.value().text()} == "STAR");
}

TEST_CASE("ClientTag: from_packed_be() rejects non-printable packed values", "[domain][shared][client_tag]") {
    // 0x00000000 — all NUL bytes
    REQUIRE_FALSE(ClientTag::from_packed_be(0x00000000u).has_value());
    // 0x53540152 — byte[2] = 0x01 (non-printable)
    REQUIRE_FALSE(ClientTag::from_packed_be(0x53540152u).has_value());
}

TEST_CASE("ClientTag: constexpr array constructor and bytes()", "[domain][shared][client_tag]") {
    constexpr ClientTag t{{'W','A','R','3'}};
    static_assert(t.packed_be() == 0x57415233u);
    REQUIRE(t.bytes()[0] == 'W');
    REQUIRE(t.bytes()[1] == 'A');
    REQUIRE(t.bytes()[2] == 'R');
    REQUIRE(t.bytes()[3] == '3');
}

TEST_CASE("ClientTag: default-constructed tag has spaces", "[domain][shared][client_tag]") {
    ClientTag t;
    REQUIRE(std::string{t.text()} == "    ");
}

// ---------------------------------------------------------------------------
// Known constants — packed_be() values match legacy CLIENTTAG_*_UINT macros
// ---------------------------------------------------------------------------

TEST_CASE("ClientTag constants: Blizzard client tags packed_be()", "[domain][shared][client_tag]") {
    // Values taken directly from src/common/tag.h CLIENTTAG_*_UINT defines
    REQUIRE(kBnChatBot.packed_be()   == 0x43484154u);  // CHAT
    REQUIRE(kStarcraft.packed_be()   == 0x53544152u);  // STAR
    REQUIRE(kBroodWar.packed_be()    == 0x53455850u);  // SEXP
    REQUIRE(kShareware.packed_be()   == 0x53534852u);  // SSHR
    REQUIRE(kDiabloRtl.packed_be()   == 0x4452544Cu);  // DRTL
    REQUIRE(kDiabloShr.packed_be()   == 0x44534852u);  // DSHR
    REQUIRE(kWarcraftII.packed_be()  == 0x5732424Eu);  // W2BN
    REQUIRE(kDiablo2.packed_be()     == 0x44324456u);  // D2DV
    REQUIRE(kStarcraftJp.packed_be() == 0x4A535452u);  // JSTR
    REQUIRE(kDiablo2St.packed_be()   == 0x44325354u);  // D2ST
    REQUIRE(kDiablo2Xp.packed_be()   == 0x44325850u);  // D2XP
    REQUIRE(kWarcraft3.packed_be()   == 0x57415233u);  // WAR3
    REQUIRE(kWar3Xp.packed_be()      == 0x57335850u);  // W3XP
    REQUIRE(kIIrc.packed_be()        == 0x49495243u);  // IIRC
    REQUIRE(kUnknown.packed_be()     == 0x554E4B4Eu);  // UNKN
}

TEST_CASE("ClientTag constants: Westwood Online client tags packed_be()", "[domain][shared][client_tag]") {
    REQUIRE(kWChat.packed_be()       == 0x57434854u);  // WCHT
    REQUIRE(kTiberianSun.packed_be() == 0x5453554Eu);  // TSUN
    REQUIRE(kTibSunXp.packed_be()    == 0x54535850u);  // TSXP
    REQUIRE(kRedAlert.packed_be()    == 0x52414C54u);  // RALT
    REQUIRE(kRedAlert2.packed_be()   == 0x52414C32u);  // RAL2
    REQUIRE(kDune2000.packed_be()    == 0x444E324Bu);  // DN2K
    REQUIRE(kNox.packed_be()         == 0x4E4F5858u);  // NOXX
    REQUIRE(kNoxQuest.packed_be()    == 0x4E4F5851u);  // NOXQ
    REQUIRE(kRenegade.packed_be()    == 0x524E4744u);  // RNGD
    REQUIRE(kRenegadeFds.packed_be() == 0x52464453u);  // RFDS
    REQUIRE(kYurisRev.packed_be()    == 0x59555249u);  // YURI
    REQUIRE(kEmperorBd.packed_be()   == 0x45424644u);  // EBFD
    REQUIRE(kLofLore3.packed_be()    == 0x4C4F5233u);  // LOR3
    REQUIRE(kWwol.packed_be()        == 0x57574F4Cu);  // WWOL
}

TEST_CASE("ClientTag constants: architecture tags packed_be()", "[domain][shared][client_tag]") {
    REQUIRE(kArchWinX86.packed_be()  == 0x49583836u);  // IX86
    REQUIRE(kArchMacPpc.packed_be()  == 0x504D4143u);  // PMAC
    REQUIRE(kArchOsxPpc.packed_be()  == 0x584D4143u);  // XMAC
}

TEST_CASE("ClientTag constants: game language tags packed_be()", "[domain][shared][client_tag]") {
    REQUIRE(kLangEnUs.packed_be()    == 0x656E5553u);  // enUS
    REQUIRE(kLangDeDE.packed_be()    == 0x64654445u);  // deDE
    REQUIRE(kLangCsCZ.packed_be()    == 0x6373435Au);  // csCZ
    REQUIRE(kLangEsES.packed_be()    == 0x65734553u);  // esES
    REQUIRE(kLangFrFR.packed_be()    == 0x66724652u);  // frFR
    REQUIRE(kLangItIT.packed_be()    == 0x69744954u);  // itIT
    REQUIRE(kLangJaJA.packed_be()    == 0x6A614A41u);  // jaJA
    REQUIRE(kLangKoKR.packed_be()    == 0x6B6F4B52u);  // koKR
    REQUIRE(kLangPlPL.packed_be()    == 0x706C504Cu);  // plPL
    REQUIRE(kLangRuRU.packed_be()    == 0x72755255u);  // ruRU
    REQUIRE(kLangZhCN.packed_be()    == 0x7A68434Eu);  // zhCN
    REQUIRE(kLangZhTW.packed_be()    == 0x7A685457u);  // zhTW
}

TEST_CASE("ClientTag constants: text() matches the 4-char string", "[domain][shared][client_tag]") {
    REQUIRE(std::string{kStarcraft.text()}   == "STAR");
    REQUIRE(std::string{kBroodWar.text()}    == "SEXP");
    REQUIRE(std::string{kWarcraft3.text()}   == "WAR3");
    REQUIRE(std::string{kWar3Xp.text()}      == "W3XP");
    REQUIRE(std::string{kDiablo2.text()}     == "D2DV");
    REQUIRE(std::string{kDiablo2Xp.text()}   == "D2XP");
    REQUIRE(std::string{kWarcraftII.text()}  == "W2BN");
    REQUIRE(std::string{kBnChatBot.text()}   == "CHAT");
    REQUIRE(std::string{kUnknown.text()}     == "UNKN");
    REQUIRE(std::string{kArchWinX86.text()}  == "IX86");
    REQUIRE(std::string{kLangEnUs.text()}    == "enUS");
}

// ---------------------------------------------------------------------------
// is_valid_client()
// ---------------------------------------------------------------------------

TEST_CASE("ClientTag: is_valid_client() returns true for all known clients", "[domain][shared][client_tag]") {
    REQUIRE(kBnChatBot.is_valid_client());
    REQUIRE(kStarcraft.is_valid_client());
    REQUIRE(kBroodWar.is_valid_client());
    REQUIRE(kShareware.is_valid_client());
    REQUIRE(kDiabloRtl.is_valid_client());
    REQUIRE(kDiabloShr.is_valid_client());
    REQUIRE(kWarcraftII.is_valid_client());
    REQUIRE(kDiablo2.is_valid_client());
    REQUIRE(kStarcraftJp.is_valid_client());
    REQUIRE(kDiablo2St.is_valid_client());
    REQUIRE(kDiablo2Xp.is_valid_client());
    REQUIRE(kWarcraft3.is_valid_client());
    REQUIRE(kWar3Xp.is_valid_client());
    REQUIRE(kIIrc.is_valid_client());
    REQUIRE(kWChat.is_valid_client());
    REQUIRE(kTiberianSun.is_valid_client());
    REQUIRE(kTibSunXp.is_valid_client());
    REQUIRE(kRedAlert.is_valid_client());
    REQUIRE(kRedAlert2.is_valid_client());
    REQUIRE(kDune2000.is_valid_client());
    REQUIRE(kNox.is_valid_client());
    REQUIRE(kNoxQuest.is_valid_client());
    REQUIRE(kRenegade.is_valid_client());
    REQUIRE(kRenegadeFds.is_valid_client());
    REQUIRE(kYurisRev.is_valid_client());
    REQUIRE(kEmperorBd.is_valid_client());
    REQUIRE(kLofLore3.is_valid_client());
    REQUIRE(kWwol.is_valid_client());
}

TEST_CASE("ClientTag: is_valid_client() returns false for non-client tags", "[domain][shared][client_tag]") {
    REQUIRE_FALSE(kUnknown.is_valid_client());
    REQUIRE_FALSE(kArchWinX86.is_valid_client());
    REQUIRE_FALSE(kArchMacPpc.is_valid_client());
    REQUIRE_FALSE(kLangEnUs.is_valid_client());
    REQUIRE_FALSE(ClientTag{}.is_valid_client());
}

// ---------------------------------------------------------------------------
// is_valid_arch()
// ---------------------------------------------------------------------------

TEST_CASE("ClientTag: is_valid_arch() returns true for architecture tags", "[domain][shared][client_tag]") {
    REQUIRE(kArchWinX86.is_valid_arch());
    REQUIRE(kArchMacPpc.is_valid_arch());
    REQUIRE(kArchOsxPpc.is_valid_arch());
}

TEST_CASE("ClientTag: is_valid_arch() returns false for non-arch tags", "[domain][shared][client_tag]") {
    REQUIRE_FALSE(kStarcraft.is_valid_arch());
    REQUIRE_FALSE(kWarcraft3.is_valid_arch());
    REQUIRE_FALSE(kUnknown.is_valid_arch());
    REQUIRE_FALSE(kLangEnUs.is_valid_arch());
}

// ---------------------------------------------------------------------------
// is_wol_v1() / is_wol_v2()
// ---------------------------------------------------------------------------

TEST_CASE("ClientTag: is_wol_v1() only matches WCHT", "[domain][shared][client_tag]") {
    REQUIRE(kWChat.is_wol_v1());
    REQUIRE_FALSE(kTiberianSun.is_wol_v1());
    REQUIRE_FALSE(kStarcraft.is_wol_v1());
    REQUIRE_FALSE(kWwol.is_wol_v1());
}

TEST_CASE("ClientTag: is_wol_v2() matches all WOL v2 clients", "[domain][shared][client_tag]") {
    REQUIRE(kTiberianSun.is_wol_v2());
    REQUIRE(kTibSunXp.is_wol_v2());
    REQUIRE(kRedAlert.is_wol_v2());
    REQUIRE(kRedAlert2.is_wol_v2());
    REQUIRE(kDune2000.is_wol_v2());
    REQUIRE(kNox.is_wol_v2());
    REQUIRE(kNoxQuest.is_wol_v2());
    REQUIRE(kRenegade.is_wol_v2());
    REQUIRE(kRenegadeFds.is_wol_v2());
    REQUIRE(kYurisRev.is_wol_v2());
    REQUIRE(kEmperorBd.is_wol_v2());
    REQUIRE(kLofLore3.is_wol_v2());
    REQUIRE(kWwol.is_wol_v2());
    // WOL v1 is NOT v2
    REQUIRE_FALSE(kWChat.is_wol_v2());
    // Blizzard clients are not WOL v2
    REQUIRE_FALSE(kStarcraft.is_wol_v2());
    REQUIRE_FALSE(kWarcraft3.is_wol_v2());
}

// ---------------------------------------------------------------------------
// title()
// ---------------------------------------------------------------------------

TEST_CASE("ClientTag: title() returns correct product names", "[domain][shared][client_tag]") {
    REQUIRE(std::string{kStarcraft.title()}   == "Starcraft");
    REQUIRE(std::string{kBroodWar.title()}    == "Starcraft: Brood War");
    REQUIRE(std::string{kWarcraft3.title()}   == "Warcraft III");
    REQUIRE(std::string{kWar3Xp.title()}      == "Warcraft III: The Frozen Throne");
    REQUIRE(std::string{kDiablo2.title()}     == "Diablo II");
    REQUIRE(std::string{kDiablo2Xp.title()}   == "Diablo II: Lord of Destruction");
    REQUIRE(std::string{kWarcraftII.title()}  == "Warcraft II");
    REQUIRE(std::string{kDiabloRtl.title()}   == "Diablo I");
    REQUIRE(std::string{kDiabloShr.title()}   == "Diablo I (Shareware)");
    REQUIRE(std::string{kShareware.title()}   == "Starcraft (Shareware)");
    REQUIRE(std::string{kBnChatBot.title()}   == "Chat");
    REQUIRE(std::string{kIIrc.title()}        == "Internet Relay Chat");
    REQUIRE(std::string{kWChat.title()}       == "Westwood Chat");
    REQUIRE(std::string{kTiberianSun.title()} == "Tiberian Sun");
    REQUIRE(std::string{kTibSunXp.title()}    == "Tiberian Sun: Firestorm");
    REQUIRE(std::string{kRedAlert.title()}    == "Red Alert");
    REQUIRE(std::string{kRedAlert2.title()}   == "Red Alert 2");
    REQUIRE(std::string{kDune2000.title()}    == "Dune 2000");
    REQUIRE(std::string{kNox.title()}         == "Nox");
    REQUIRE(std::string{kNoxQuest.title()}    == "Nox Quest");
    REQUIRE(std::string{kRenegade.title()}    == "Renegade");
    REQUIRE(std::string{kRenegadeFds.title()} == "Renegade Free Dedicated Server");
    REQUIRE(std::string{kYurisRev.title()}    == "Yuri's Revenge");
    REQUIRE(std::string{kEmperorBd.title()}   == "Emperor: Battle for Dune");
    REQUIRE(std::string{kLofLore3.title()}    == "Lands of Lore 3");
    REQUIRE(std::string{kWwol.title()}        == "Westwood Online");
}

TEST_CASE("ClientTag: title() returns 'Unknown' for unrecognised tags", "[domain][shared][client_tag]") {
    REQUIRE(std::string{kUnknown.title()}     == "Unknown");
    REQUIRE(std::string{kArchWinX86.title()}  == "Unknown");
    REQUIRE(std::string{kLangEnUs.title()}    == "Unknown");
    REQUIRE(std::string{ClientTag{}.title()}  == "Unknown");
}

// ---------------------------------------------------------------------------
// Comparison operators
// ---------------------------------------------------------------------------

TEST_CASE("ClientTag: equality and inequality", "[domain][shared][client_tag]") {
    REQUIRE(kStarcraft == kStarcraft);
    REQUIRE_FALSE(kStarcraft == kBroodWar);
    REQUIRE(kStarcraft != kBroodWar);
    REQUIRE_FALSE(kStarcraft != kStarcraft);
}

TEST_CASE("ClientTag: spaceship operator provides total order", "[domain][shared][client_tag]") {
    // STAR (0x53544152) < WAR3 (0x57415233) lexicographically
    REQUIRE(kStarcraft < kWarcraft3);
    REQUIRE(kWarcraft3 > kStarcraft);
    REQUIRE(kStarcraft <= kStarcraft);
    REQUIRE(kStarcraft >= kStarcraft);
}

TEST_CASE("ClientTag: parse() result equals matching constant", "[domain][shared][client_tag]") {
    auto t = ClientTag::parse("WAR3");
    REQUIRE(t.has_value());
    REQUIRE(t.value() == kWarcraft3);
}

TEST_CASE("ClientTag: from_packed_be() result equals matching constant", "[domain][shared][client_tag]") {
    auto t = ClientTag::from_packed_be(0x57335850u);  // W3XP
    REQUIRE(t.has_value());
    REQUIRE(t.value() == kWar3Xp);
}

// ---------------------------------------------------------------------------
// std::hash
// ---------------------------------------------------------------------------

TEST_CASE("ClientTag: std::hash works in unordered_set", "[domain][shared][client_tag]") {
    std::unordered_set<ClientTag> s;
    s.insert(kStarcraft);
    s.insert(kBroodWar);
    s.insert(kWarcraft3);
    s.insert(kStarcraft);  // duplicate — should not increase size

    REQUIRE(s.size() == 3);
    REQUIRE(s.count(kStarcraft) == 1);
    REQUIRE(s.count(kWar3Xp)    == 0);
}

TEST_CASE("ClientTag: equal tags produce equal hashes", "[domain][shared][client_tag]") {
    std::hash<ClientTag> h;
    REQUIRE(h(kStarcraft) == h(kStarcraft));
    REQUIRE(h(kWarcraft3) == h(kWarcraft3));
}
