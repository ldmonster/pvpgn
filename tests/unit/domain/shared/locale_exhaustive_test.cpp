// SPDX-License-Identifier: GPL-2.0-or-later

/// @file locale_exhaustive_test.cpp
/// Unit tests for domain::Locale. There was no prior locale test file,
/// so this exercises every branch of Locale::parse_or_default plus the
/// fallback constant, text() round-trip, is_default(), and the defaulted
/// spaceship operator.
///
/// Note: Locale never fails — unparseable input falls back to enUS — so all
/// assertions are on the resulting Locale value, not on a Result.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "domain/shared/locale.hpp"

using pvpgn::domain::Locale;

// ---------------------------------------------------------------------------
// Default construction & fallback constant
// ---------------------------------------------------------------------------

TEST_CASE("Locale: default-constructed equals enUS fallback",
          "[domain][shared][locale]") {
    Locale l;
    REQUIRE(l.is_default());
    REQUIRE(std::string{l.text()} == "enUS");
}

TEST_CASE("Locale: kFallback constant spells enUS",
          "[domain][shared][locale]") {
    REQUIRE(Locale::kFallback[0] == 'e');
    REQUIRE(Locale::kFallback[1] == 'n');
    REQUIRE(Locale::kFallback[2] == 'U');
    REQUIRE(Locale::kFallback[3] == 'S');
}

// ---------------------------------------------------------------------------
// Accepting the canonical xxYY form (two lower + two upper)
// ---------------------------------------------------------------------------

TEST_CASE("Locale: parse_or_default accepts known locales and round-trips text()",
          "[domain][shared][locale]") {
    REQUIRE(std::string{Locale::parse_or_default("enUS").text()} == "enUS");
    REQUIRE(std::string{Locale::parse_or_default("deDE").text()} == "deDE");
    REQUIRE(std::string{Locale::parse_or_default("frFR").text()} == "frFR");
    REQUIRE(std::string{Locale::parse_or_default("ruRU").text()} == "ruRU");
    REQUIRE(std::string{Locale::parse_or_default("zhCN").text()} == "zhCN");
}

TEST_CASE("Locale: a parsed non-enUS locale is not the default",
          "[domain][shared][locale]") {
    auto de = Locale::parse_or_default("deDE");
    REQUIRE_FALSE(de.is_default());
}

TEST_CASE("Locale: parsing enUS explicitly still reports is_default",
          "[domain][shared][locale]") {
    REQUIRE(Locale::parse_or_default("enUS").is_default());
}

// ---------------------------------------------------------------------------
// Fallback branches — each malformed shape returns enUS
// ---------------------------------------------------------------------------

TEST_CASE("Locale: empty string falls back to enUS",
          "[domain][shared][locale]") {
    REQUIRE(Locale::parse_or_default("").is_default());
}

TEST_CASE("Locale: too-short input falls back",
          "[domain][shared][locale]") {
    REQUIRE(Locale::parse_or_default("en").is_default());
    REQUIRE(Locale::parse_or_default("enU").is_default());
}

TEST_CASE("Locale: too-long input falls back",
          "[domain][shared][locale]") {
    REQUIRE(Locale::parse_or_default("enUSX").is_default());
    REQUIRE(Locale::parse_or_default("english").is_default());
}

TEST_CASE("Locale: wrong case pattern — all upper — falls back",
          "[domain][shared][locale]") {
    REQUIRE(Locale::parse_or_default("ENUS").is_default());
}

TEST_CASE("Locale: wrong case pattern — all lower — falls back",
          "[domain][shared][locale]") {
    REQUIRE(Locale::parse_or_default("enus").is_default());
}

TEST_CASE("Locale: first char uppercase violates lower-lower-upper-upper rule",
          "[domain][shared][locale]") {
    REQUIRE(Locale::parse_or_default("EnUS").is_default());
}

TEST_CASE("Locale: second char uppercase falls back",
          "[domain][shared][locale]") {
    REQUIRE(Locale::parse_or_default("eNUS").is_default());
}

TEST_CASE("Locale: third char lowercase falls back",
          "[domain][shared][locale]") {
    REQUIRE(Locale::parse_or_default("enuS").is_default());
}

TEST_CASE("Locale: fourth char lowercase falls back",
          "[domain][shared][locale]") {
    REQUIRE(Locale::parse_or_default("enUs").is_default());
}

TEST_CASE("Locale: digits in place of letters fall back",
          "[domain][shared][locale]") {
    REQUIRE(Locale::parse_or_default("12US").is_default());
    REQUIRE(Locale::parse_or_default("en12").is_default());
}

TEST_CASE("Locale: punctuation falls back",
          "[domain][shared][locale]") {
    REQUIRE(Locale::parse_or_default("en-S").is_default());
}

// ---------------------------------------------------------------------------
// Equality / ordering via defaulted operator<=>
// ---------------------------------------------------------------------------

TEST_CASE("Locale: equal locales compare equal",
          "[domain][shared][locale]") {
    REQUIRE(Locale::parse_or_default("frFR") == Locale::parse_or_default("frFR"));
}

TEST_CASE("Locale: different locales compare unequal",
          "[domain][shared][locale]") {
    REQUIRE(Locale::parse_or_default("frFR") != Locale::parse_or_default("deDE"));
}

TEST_CASE("Locale: ordering follows lexicographic byte order",
          "[domain][shared][locale]") {
    // 'd' (0x64) < 'e' (0x65) so deDE < enUS
    REQUIRE(Locale::parse_or_default("deDE") < Locale::parse_or_default("enUS"));
    REQUIRE(Locale::parse_or_default("enUS") > Locale::parse_or_default("deDE"));
    auto a = Locale::parse_or_default("frFR");
    REQUIRE(a <= a);
    REQUIRE(a >= a);
}

// ---------------------------------------------------------------------------
// constexpr usability
// ---------------------------------------------------------------------------

TEST_CASE("Locale: default construction and text() are usable at compile time",
          "[domain][shared][locale]") {
    constexpr Locale l;
    static_assert(l.is_default());
    static_assert(l.text().size() == 4);
    REQUIRE(true);
}
