// SPDX-License-Identifier: GPL-2.0-or-later
#include <array>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "application/i18n/string_table.hpp"

using pvpgn::application::i18n::MapStringTable;

TEST_CASE("string_table: exact locale match wins",
          "[application][i18n]") {
    MapStringTable t;
    t.set("enUS", "hello", "Hello!");
    t.set("ruRU", "hello", "Privet!");
    REQUIRE(t.format("hello", "enUS") == "Hello!");
    REQUIRE(t.format("hello", "ruRU") == "Privet!");
}

TEST_CASE("string_table: empty-locale entry is the default fallback",
          "[application][i18n]") {
    MapStringTable t;
    t.set("", "hello", "Hi.");
    t.set("ruRU", "hello", "Privet!");
    REQUIRE(t.format("hello", "ruRU") == "Privet!");
    REQUIRE(t.format("hello", "frFR") == "Hi.");
    REQUIRE(t.format("hello", "") == "Hi.");
}

TEST_CASE("string_table: locale matching is case-insensitive",
          "[application][i18n]") {
    MapStringTable t;
    t.set("enUS", "k", "ok");
    REQUIRE(t.format("k", "ENUS") == "ok");
    REQUIRE(t.format("k", "enus") == "ok");
}

TEST_CASE("string_table: unknown key returns the key itself",
          "[application][i18n]") {
    MapStringTable t;
    REQUIRE(t.format("missing.key", "enUS") == "missing.key");
}

TEST_CASE("string_table: positional args substitute {0}..{9}",
          "[application][i18n]") {
    MapStringTable t;
    t.set("", "k", "{0} -> {1}");
    std::array<std::string_view, 2> args{"a", "b"};
    REQUIRE(t.format("k", "", args) == "a -> b");
}

TEST_CASE("string_table: unmatched placeholders survive verbatim",
          "[application][i18n]") {
    MapStringTable t;
    t.set("", "k", "x={0} y={5} z={0}");
    std::array<std::string_view, 1> args{"A"};
    REQUIRE(t.format("k", "", args) == "x=A y={5} z=A");
}

TEST_CASE("string_table: re-set overwrites the previous template",
          "[application][i18n]") {
    MapStringTable t;
    t.set("enUS", "k", "first");
    t.set("enUS", "k", "second");
    REQUIRE(t.format("k", "enUS") == "second");
}

TEST_CASE("string_table: language-only fallback when country is unknown",
          "[application][i18n]") {
    MapStringTable t;
    t.set("ru", "k", "ru-text");
    REQUIRE(t.format("k", "ruRU") == "ru-text");
    REQUIRE(t.format("k", "ruUA") == "ru-text");
}

TEST_CASE("string_table: country-specific entry beats language-only",
          "[application][i18n]") {
    MapStringTable t;
    t.set("ru",   "k", "ru-text");
    t.set("ruRU", "k", "ruRU-text");
    REQUIRE(t.format("k", "ruRU") == "ruRU-text");
    REQUIRE(t.format("k", "ruUA") == "ru-text");
}
