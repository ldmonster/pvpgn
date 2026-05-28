// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>

#include <catch2/catch_test_macros.hpp>

#include "infra/inmemory/in_memory_helpfile_source.hpp"

namespace pvpgn::infra::inmemory {

TEST_CASE("InMemoryHelpfileSource: LookupUnknownCommandReturnsNullopt", "[infra][inmemory]") {
    InMemoryHelpfileSource source;
    auto result = source.lookup("kick");
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("InMemoryHelpfileSource: SetAndLookup", "[infra][inmemory]") {
    InMemoryHelpfileSource source;
    source.set("kick", "Usage: /kick <user>");

    auto result = source.lookup("kick");
    REQUIRE(result.has_value());
    REQUIRE(result.value() == "Usage: /kick <user>");
}

TEST_CASE("InMemoryHelpfileSource: OverwriteExistingEntry", "[infra][inmemory]") {
    InMemoryHelpfileSource source;
    source.set("ban", "old text");
    source.set("ban", "new text");

    auto result = source.lookup("ban");
    REQUIRE(result.has_value());
    REQUIRE(result.value() == "new text");
}

TEST_CASE("InMemoryHelpfileSource: AllCommandsEmptyWhenNoEntries", "[infra][inmemory]") {
    InMemoryHelpfileSource source;
    auto cmds = source.all_commands();
    REQUIRE(cmds.empty());
}

TEST_CASE("InMemoryHelpfileSource: AllCommandsReturnsRegisteredNames", "[infra][inmemory]") {
    InMemoryHelpfileSource source;
    source.set("kick",  "kick help");
    source.set("ban",   "ban help");
    source.set("unban", "unban help");

    auto cmds = source.all_commands();
    REQUIRE(cmds.size() == 3);

    // Order is unspecified (unordered_map), so check membership
    REQUIRE(std::find(cmds.begin(), cmds.end(), "kick")  != cmds.end());
    REQUIRE(std::find(cmds.begin(), cmds.end(), "ban")   != cmds.end());
    REQUIRE(std::find(cmds.begin(), cmds.end(), "unban") != cmds.end());
}

TEST_CASE("InMemoryHelpfileSource: LookupIsCaseSensitive", "[infra][inmemory]") {
    InMemoryHelpfileSource source;
    source.set("Kick", "Kick help");

    REQUIRE_FALSE(source.lookup("kick").has_value());
    REQUIRE(source.lookup("Kick").has_value());
}

TEST_CASE("InMemoryHelpfileSource: MultipleEntriesDoNotInterfere", "[infra][inmemory]") {
    InMemoryHelpfileSource source;
    source.set("cmd1", "help for cmd1");
    source.set("cmd2", "help for cmd2");

    REQUIRE(source.lookup("cmd1").value() == "help for cmd1");
    REQUIRE(source.lookup("cmd2").value() == "help for cmd2");
    REQUIRE_FALSE(source.lookup("cmd3").has_value());
}

}  // namespace pvpgn::infra::inmemory
