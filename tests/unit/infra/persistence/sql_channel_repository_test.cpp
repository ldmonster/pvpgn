// SPDX-License-Identifier: GPL-2.0-or-later
//
// tests/unit/infra/persistence/sql_channel_repository_test.cpp
//
// Verifies SqlChannelRepository over the recording fake IDbDriver (no sqlite
// needed). Pins SQL generation / row rehydration and — for channel-routing F6 —
// that name lookup is emitted case-insensitively (COLLATE NOCASE), mirroring
// the original server's strcasecmp-based channellist_find_channel_by_name.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "domain/chat/channel.hpp"
#include "infra/persistence/channel_repository.hpp"

#include "recording_fake_driver.hpp"

using namespace pvpgn::infra::persistence;
using namespace pvpgn::test::persistence;
using pvpgn::domain::ChannelId;
using pvpgn::domain::chat::Channel;

namespace {

std::shared_ptr<RecordingFakeDriver> make_driver() {
    return std::make_shared<RecordingFakeDriver>();
}

// Build a channel row: id, name, topic, flags, max_members.
FakeRow channel_row(std::int64_t id, std::string name, std::string topic,
                    std::int64_t flags, std::int64_t max_members) {
    return FakeRow{std::vector<Cell>{id, std::move(name), std::move(topic),
                                     flags, max_members}};
}

}  // namespace

TEST_CASE("SqlChannelRepository::find_by_name matches case-insensitively (F6)",
          "[infra][persistence][channel]") {
    // Regression for channel-routing F6: the original uses strcasecmp; the SQL
    // lookup must carry COLLATE NOCASE so "war3" resolves the row stored as
    // "War3" instead of missing and spawning a duplicate channel.
    auto driver = make_driver();
    SqlChannelRepository repo{driver};

    // The row the DB returns keeps its original "War3" casing.
    driver->next_rows.push_back(channel_row(42, "War3", "", 0, 0));

    auto found = repo.find_by_name("war3");  // queried with different case
    REQUIRE(found.has_value());
    CHECK(found.value().id().value() == 42u);
    // Display name preserves the original casing (not lowercased).
    CHECK(found.value().name() == "War3");

    REQUIRE(driver->calls.size() == 1);
    const auto& call = driver->last();
    // The comparison must be case-insensitive.
    CHECK(call.sql.find("COLLATE NOCASE") != std::string::npos);
    REQUIRE(call.params.size() == 1);
    CHECK(as_str(call.params[0]) == "war3");
}

TEST_CASE("SqlChannelRepository::save issues a bound upsert preserving casing",
          "[infra][persistence][channel]") {
    auto driver = make_driver();
    SqlChannelRepository repo{driver};

    auto ch = Channel::create(ChannelId{7}, "War3",
                              pvpgn::domain::chat::ChannelPolicy{});
    REQUIRE(repo.save(ch).has_value());

    REQUIRE(driver->calls.size() == 1);
    const auto& call = driver->last();
    CHECK(call.sql.find("INSERT OR REPLACE INTO channels") != std::string::npos);
    // The statement must use bound placeholders, never the literal value
    // concatenated into the SQL text.
    CHECK(call.sql.find("VALUES (?, ?, ?, ?, ?)") != std::string::npos);
    CHECK(call.sql.find("'War3'") == std::string::npos);
    // Columns: id, name, topic, flags, max_members — name is bound, casing kept.
    REQUIRE(call.params.size() == 5);
    CHECK(as_int(call.params[0]) == 7);
    CHECK(as_str(call.params[1]) == "War3");
    CHECK(as_str(call.params[2]).empty());  // topic
}

TEST_CASE("SqlChannelRepository::save binds SQL metacharacters instead of "
          "concatenating them (injection regression)",
          "[infra][persistence][channel][security]") {
    // Regression for the CRIT stacked-query SQL injection: a channel whose name
    // or topic carries SQL metacharacters (quote, semicolon, comment, a full
    // `'); DROP TABLE x;--` payload) used to be concatenated raw into an
    // INSERT run through sqlite3_exec, allowing arbitrary stacked DDL/DML.
    // The fix passes both values as bound parameters; this test pins that the
    // payload never appears in the emitted SQL text and round-trips intact as a
    // single bound param.
    auto driver = make_driver();
    SqlChannelRepository repo{driver};

    const std::string evil_name  = "x'); DROP TABLE accounts;--";
    const std::string evil_topic =
        "gg', 0, 0); UPDATE accounts SET command_groups='1,2' "
        "WHERE name='attacker';--";

    // rehydrate lets us seed a topic directly (create() takes no topic).
    auto ch = Channel::rehydrate(ChannelId{13}, evil_name, evil_topic,
                                 pvpgn::domain::chat::ChannelPolicy{}, {}, {});
    REQUIRE(repo.save(ch).has_value());

    REQUIRE(driver->calls.size() == 1);
    const auto& call = driver->last();

    // (a) The SQL text is exactly the parameterized statement — no fragment of
    //     the malicious payload (and none of its metacharacters) is present.
    CHECK(call.sql.find("VALUES (?, ?, ?, ?, ?)") != std::string::npos);
    CHECK(call.sql.find("DROP TABLE") == std::string::npos);
    CHECK(call.sql.find("UPDATE accounts") == std::string::npos);
    CHECK(call.sql.find(";--") == std::string::npos);
    // No quote/semicolon leaked from a value into the statement. The only
    // semicolons that could exist would come from the payload; the placeholder
    // statement has none.
    CHECK(call.sql.find('\'') == std::string::npos);
    CHECK(call.sql.find(';') == std::string::npos);

    // (b) The malicious values round-trip intact as bound parameters, in order:
    //     id, name, topic, flags, max_members.
    REQUIRE(call.params.size() == 5);
    CHECK(as_int(call.params[0]) == 13);
    CHECK(as_str(call.params[1]) == evil_name);   // quote/`;`/`--` preserved
    CHECK(as_str(call.params[2]) == evil_topic);
}

TEST_CASE("SqlChannelRepository::remove binds the id (no concatenation)",
          "[infra][persistence][channel][security]") {
    auto driver = make_driver();
    SqlChannelRepository repo{driver};

    REQUIRE(repo.remove(ChannelId{99}).has_value());

    REQUIRE(driver->calls.size() == 1);
    const auto& call = driver->last();
    CHECK(call.sql.find("DELETE FROM channels WHERE id = ?") != std::string::npos);
    REQUIRE(call.params.size() == 1);
    CHECK(as_int(call.params[0]) == 99);
}
