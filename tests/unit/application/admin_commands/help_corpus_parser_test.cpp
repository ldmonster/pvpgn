// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `parse_help_corpus` and `HelpCorpus::find_by_alias`.
//
// The parser is the replacement for the legacy
// `helpfile.cpp` reader. The fixtures below mirror the shapes of
// actual lines from `conf/bnhelp.conf.in`.

#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "application/admin_commands/help_corpus.hpp"
#include "application/admin_commands/help_corpus_parser.hpp"

namespace ac = pvpgn::application::admin_commands;

namespace {

ac::HelpCorpus parse_or_fail(std::string_view text) {
    std::stringstream ss{std::string{text}};
    auto r = ac::parse_help_corpus(ss);
    REQUIRE(r);
    return std::move(r).value();
}

}  // namespace

TEST_CASE("parser: empty input yields empty corpus", "[help_corpus]") {
    const auto corpus = parse_or_fail("");
    REQUIRE(corpus.empty());
}

TEST_CASE("parser: single entry with no body", "[help_corpus]") {
    const auto corpus = parse_or_fail("%help\n");
    REQUIRE(corpus.size() == 1);
    REQUIRE(corpus.entries()[0].aliases.size() == 1);
    REQUIRE(corpus.entries()[0].aliases[0] == "/help");
    REQUIRE(corpus.entries()[0].description_lines.empty());
}

TEST_CASE("parser: aliases on header are split and slash-prefixed", "[help_corpus]") {
    const auto corpus = parse_or_fail("%help ? h\n  body line\n");
    REQUIRE(corpus.size() == 1);
    const auto& e = corpus.entries()[0];
    REQUIRE(e.aliases.size() == 3);
    REQUIRE(e.aliases[0] == "/help");
    REQUIRE(e.aliases[1] == "/?");
    REQUIRE(e.aliases[2] == "/h");
}

TEST_CASE("parser: trailing `# comment` on header is stripped", "[help_corpus]") {
    const auto corpus = parse_or_fail("%who w   # list users in channel\n");
    REQUIRE(corpus.size() == 1);
    REQUIRE(corpus.entries()[0].aliases.size() == 2);
    REQUIRE(corpus.entries()[0].aliases[0] == "/who");
    REQUIRE(corpus.entries()[0].aliases[1] == "/w");
}

TEST_CASE("parser: full-line `#` comment is dropped from body", "[help_corpus]") {
    const auto corpus = parse_or_fail(
        "%help\n"
        "# this is just a note\n"
        "body line\n");
    REQUIRE(corpus.size() == 1);
    REQUIRE(corpus.entries()[0].description_lines.size() == 1);
    REQUIRE(corpus.entries()[0].description_lines[0] == "body line");
}

TEST_CASE("parser: trailing `# ...` on body is truncated", "[help_corpus]") {
    const auto corpus = parse_or_fail(
        "%help\n"
        "describe usage   # internal todo\n");
    REQUIRE(corpus.size() == 1);
    REQUIRE(corpus.entries()[0].description_lines.size() == 1);
    // Truncation runs at the first `#`; trailing spaces before the
    // `#` survive intact (legacy behaviour: it terminates at `#`
    // without further trimming).
    REQUIRE(corpus.entries()[0].description_lines[0] == "describe usage   ");
}

TEST_CASE("parser: tabs in body are expanded to three spaces", "[help_corpus]") {
    const auto corpus = parse_or_fail(
        "%help\n"
        "\tindented\n");
    REQUIRE(corpus.size() == 1);
    REQUIRE(corpus.entries()[0].description_lines.size() == 1);
    REQUIRE(corpus.entries()[0].description_lines[0] == "   indented");
}

TEST_CASE("parser: blank lines in body are dropped", "[help_corpus]") {
    const auto corpus = parse_or_fail(
        "%help\n"
        "first\n"
        "\n"
        "   \n"
        "second\n");
    REQUIRE(corpus.size() == 1);
    REQUIRE(corpus.entries()[0].description_lines.size() == 2);
    REQUIRE(corpus.entries()[0].description_lines[0] == "first");
    REQUIRE(corpus.entries()[0].description_lines[1] == "second");
}

TEST_CASE("parser: multiple entries", "[help_corpus]") {
    const auto corpus = parse_or_fail(
        "%help\n"
        "show help\n"
        "%who\n"
        "list users\n"
        "and channels\n");
    REQUIRE(corpus.size() == 2);
    REQUIRE(corpus.entries()[0].aliases[0] == "/help");
    REQUIRE(corpus.entries()[0].description_lines.size() == 1);
    REQUIRE(corpus.entries()[1].aliases[0] == "/who");
    REQUIRE(corpus.entries()[1].description_lines.size() == 2);
}

TEST_CASE("parser: pre-header lines are ignored", "[help_corpus]") {
    const auto corpus = parse_or_fail(
        "garbage line\n"
        "# a comment before any %\n"
        "%help\n"
        "body\n");
    REQUIRE(corpus.size() == 1);
    REQUIRE(corpus.entries()[0].aliases[0] == "/help");
    REQUIRE(corpus.entries()[0].description_lines.size() == 1);
}

TEST_CASE("parser: CRLF line endings are handled", "[help_corpus]") {
    const auto corpus = parse_or_fail(
        "%help ?\r\n"
        "body\r\n");
    REQUIRE(corpus.size() == 1);
    REQUIRE(corpus.entries()[0].aliases.size() == 2);
    REQUIRE(corpus.entries()[0].aliases[1] == "/?");
    REQUIRE(corpus.entries()[0].description_lines[0] == "body");
}

TEST_CASE("parser: leading whitespace before %% header is tolerated", "[help_corpus]") {
    const auto corpus = parse_or_fail("   %help\nbody\n");
    REQUIRE(corpus.size() == 1);
    REQUIRE(corpus.entries()[0].aliases[0] == "/help");
}

TEST_CASE("find_by_alias: matches canonical name", "[help_corpus]") {
    const auto corpus = parse_or_fail("%help ?\nbody\n");
    const auto* e = corpus.find_by_alias("/help");
    REQUIRE(e != nullptr);
    REQUIRE(e->aliases[0] == "/help");
}

TEST_CASE("find_by_alias: matches alias", "[help_corpus]") {
    const auto corpus = parse_or_fail("%help ?\nbody\n");
    const auto* e = corpus.find_by_alias("/?");
    REQUIRE(e != nullptr);
    REQUIRE(e->aliases[0] == "/help");
}

TEST_CASE("find_by_alias: matches without leading slash", "[help_corpus]") {
    const auto corpus = parse_or_fail("%help\nbody\n");
    const auto* e = corpus.find_by_alias("help");
    REQUIRE(e != nullptr);
}

TEST_CASE("find_by_alias: is case-insensitive", "[help_corpus]") {
    const auto corpus = parse_or_fail("%Help\nbody\n");
    const auto* e = corpus.find_by_alias("/HELP");
    REQUIRE(e != nullptr);
    e = corpus.find_by_alias("help");
    REQUIRE(e != nullptr);
}

TEST_CASE("find_by_alias: returns nullptr on miss", "[help_corpus]") {
    const auto corpus = parse_or_fail("%help\nbody\n");
    REQUIRE(corpus.find_by_alias("/nope") == nullptr);
    REQUIRE(corpus.find_by_alias("") == nullptr);
}
