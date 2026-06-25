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
    // The stored display name keeps its original casing.
    CHECK(call.sql.find("'War3'") != std::string::npos);
}
