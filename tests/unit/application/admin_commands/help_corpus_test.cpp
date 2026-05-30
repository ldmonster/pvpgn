// SPDX-License-Identifier: GPL-2.0-or-later
//
// Unit tests for HelpCorpus::find_by_alias().
//
// The parser tests (help_corpus_parser_test.cpp) already exercise the
// round-trip from text → corpus.  These tests focus on the lookup
// semantics of the in-memory model directly.

#include <catch2/catch_test_macros.hpp>

#include "application/admin_commands/help_corpus.hpp"

namespace ac = pvpgn::application::admin_commands;

namespace {

/// Build a minimal corpus with two entries for testing.
ac::HelpCorpus make_corpus() {
    ac::HelpCorpus c;

    ac::HelpEntry help_entry;
    help_entry.aliases           = {"/help", "/?", "/h"};
    help_entry.description_lines = {"Shows help text."};
    c.add(std::move(help_entry));

    ac::HelpEntry who_entry;
    who_entry.aliases           = {"/who", "/w"};
    who_entry.description_lines = {"Lists users in channel."};
    c.add(std::move(who_entry));

    return c;
}

}  // namespace

TEST_CASE("HelpCorpus::find_by_alias: exact canonical match", "[help_corpus]") {
    const auto corpus = make_corpus();
    const auto* entry = corpus.find_by_alias("/help");
    REQUIRE(entry != nullptr);
    CHECK(entry->aliases[0] == "/help");
}

TEST_CASE("HelpCorpus::find_by_alias: alias match", "[help_corpus]") {
    const auto corpus = make_corpus();
    const auto* entry = corpus.find_by_alias("/?");
    REQUIRE(entry != nullptr);
    CHECK(entry->aliases[0] == "/help");
}

TEST_CASE("HelpCorpus::find_by_alias: case-insensitive", "[help_corpus]") {
    const auto corpus = make_corpus();
    const auto* entry = corpus.find_by_alias("/HELP");
    REQUIRE(entry != nullptr);
    CHECK(entry->aliases[0] == "/help");
}

TEST_CASE("HelpCorpus::find_by_alias: strips leading slash from needle", "[help_corpus]") {
    const auto corpus = make_corpus();
    // Lookup without leading slash should still find the entry
    const auto* entry = corpus.find_by_alias("help");
    REQUIRE(entry != nullptr);
    CHECK(entry->aliases[0] == "/help");
}

TEST_CASE("HelpCorpus::find_by_alias: strips leading slash case-insensitive", "[help_corpus]") {
    const auto corpus = make_corpus();
    const auto* entry = corpus.find_by_alias("WHO");
    REQUIRE(entry != nullptr);
    CHECK(entry->aliases[0] == "/who");
}

TEST_CASE("HelpCorpus::find_by_alias: returns nullptr for unknown command", "[help_corpus]") {
    const auto corpus = make_corpus();
    const auto* entry = corpus.find_by_alias("/unknown");
    CHECK(entry == nullptr);
}

TEST_CASE("HelpCorpus::find_by_alias: empty corpus returns nullptr", "[help_corpus]") {
    const ac::HelpCorpus empty;
    CHECK(empty.find_by_alias("/help") == nullptr);
}

TEST_CASE("HelpCorpus: size and empty reflect added entries", "[help_corpus]") {
    ac::HelpCorpus corpus;
    CHECK(corpus.empty());
    CHECK(corpus.size() == 0);

    ac::HelpEntry e;
    e.aliases = {"/test"};
    corpus.add(std::move(e));

    CHECK_FALSE(corpus.empty());
    CHECK(corpus.size() == 1);
}
