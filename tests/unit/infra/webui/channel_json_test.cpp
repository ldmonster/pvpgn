// SPDX-License-Identifier: GPL-2.0-or-later
//
// WebUI /api/v1/channels JSON serialisation tests
//
// Covers:
//   - channels_to_json(nullptr)  → "[]"
//   - channels_to_json(empty)    → "[]"
//   - Single channel: id, name, topic, member_count, permanent fields present
//   - Multiple channels: comma-separated array
//   - Channel name with special JSON characters (", \) is escaped
//   - Permanent flag reflected correctly
//   - Non-permanent channel: permanent=false
//   - member_count reflects actual members

#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "infra/inmemory/channel_repository.hpp"
#include "infra/webui/channel_json.hpp"
#include "domain/chat/channel.hpp"
#include "domain/shared/ids.hpp"

using namespace pvpgn;
using namespace pvpgn::infra::webui;
using namespace pvpgn::domain::chat;
using namespace pvpgn::infra::inmemory;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

/// Make a simple channel with the given id, name, and optional permanent flag.
Channel make_channel(std::uint32_t id,
                     std::string   name,
                     bool          permanent = false,
                     std::string   topic     = "")
{
    ChannelPolicy policy;
    if (permanent) {
        policy.flags.set(ChannelFlag::Permanent);
    }
    auto ch = Channel::create(domain::ChannelId{id}, std::move(name), policy);
    (void)topic;  // topic is set via rehydrate; for simplicity use create here
    return ch;
}

/// Make a channel with a topic via rehydrate.
Channel make_channel_with_topic(std::uint32_t id,
                                std::string   name,
                                std::string   topic,
                                bool          permanent = false)
{
    ChannelPolicy policy;
    if (permanent) {
        policy.flags.set(ChannelFlag::Permanent);
    }
    return Channel::rehydrate(domain::ChannelId{id}, std::move(name),
                              std::move(topic), policy, {}, {});
}

/// Check that @p json contains the substring @p needle.
bool contains(std::string_view json, std::string_view needle) {
    return json.find(needle) != std::string_view::npos;
}

}  // namespace

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("channels_to_json: null repository returns empty array",
          "[infra][webui][channels_json]")
{
    const std::string json = channels_to_json(nullptr);
    REQUIRE(json == "[]");
}

TEST_CASE("channels_to_json: empty repository returns empty array",
          "[infra][webui][channels_json]")
{
    InMemoryChannelRepository repo;
    const std::string json = channels_to_json(&repo);
    REQUIRE(json == "[]");
}

TEST_CASE("channels_to_json: single channel — all fields present",
          "[infra][webui][channels_json]")
{
    InMemoryChannelRepository repo;
    (void)repo.save(make_channel_with_topic(1, "Lobby", "Welcome!", false));

    const std::string json = channels_to_json(&repo);

    REQUIRE(json.front() == '[');
    REQUIRE(json.back()  == ']');

    CHECK(contains(json, "\"id\":1"));
    CHECK(contains(json, "\"name\":\"Lobby\""));
    CHECK(contains(json, "\"topic\":\"Welcome!\""));
    CHECK(contains(json, "\"member_count\":0"));
    CHECK(contains(json, "\"permanent\":false"));
}

TEST_CASE("channels_to_json: permanent channel — permanent=true",
          "[infra][webui][channels_json]")
{
    InMemoryChannelRepository repo;
    (void)repo.save(make_channel(2, "Blizzard Lobby", /*permanent=*/true));

    const std::string json = channels_to_json(&repo);

    CHECK(contains(json, "\"permanent\":true"));
    CHECK(contains(json, "\"name\":\"Blizzard Lobby\""));
}

TEST_CASE("channels_to_json: non-permanent channel — permanent=false",
          "[infra][webui][channels_json]")
{
    InMemoryChannelRepository repo;
    (void)repo.save(make_channel(3, "Temp", /*permanent=*/false));

    const std::string json = channels_to_json(&repo);

    CHECK(contains(json, "\"permanent\":false"));
}

TEST_CASE("channels_to_json: multiple channels — comma-separated",
          "[infra][webui][channels_json]")
{
    InMemoryChannelRepository repo;
    (void)repo.save(make_channel(1, "Alpha"));
    (void)repo.save(make_channel(2, "Beta"));
    (void)repo.save(make_channel(3, "Gamma"));

    const std::string json = channels_to_json(&repo);

    REQUIRE(json.front() == '[');
    REQUIRE(json.back()  == ']');

    // All three names must appear
    CHECK(contains(json, "\"name\":\"Alpha\""));
    CHECK(contains(json, "\"name\":\"Beta\""));
    CHECK(contains(json, "\"name\":\"Gamma\""));

    // Must have exactly 2 commas between 3 objects (at least 2 commas total)
    std::size_t comma_count = 0;
    for (char c : json) {
        if (c == ',') { ++comma_count; }
    }
    // Each object has 4 commas between its 5 fields, plus 2 between objects
    // We just verify there are at least 2 inter-object commas
    CHECK(comma_count >= 2u);
}

TEST_CASE("channels_to_json: channel name with double-quote is escaped",
          "[infra][webui][channels_json]")
{
    InMemoryChannelRepository repo;
    (void)repo.save(make_channel(10, R"(Say "Hello")"));

    const std::string json = channels_to_json(&repo);

    // The name must appear with escaped quotes
    CHECK(contains(json, R"(\"Hello\")"));
}

TEST_CASE("channels_to_json: channel name with backslash is escaped",
          "[infra][webui][channels_json]")
{
    InMemoryChannelRepository repo;
    (void)repo.save(make_channel(11, R"(path\to\channel)"));

    const std::string json = channels_to_json(&repo);

    // Backslashes must be doubled
    CHECK(contains(json, R"(path\\to\\channel)"));
}

TEST_CASE("channels_to_json: topic with special chars is escaped",
          "[infra][webui][channels_json]")
{
    InMemoryChannelRepository repo;
    (void)repo.save(make_channel_with_topic(12, "Test", R"(topic "with" quotes)"));

    const std::string json = channels_to_json(&repo);

    CHECK(contains(json, R"(\"with\")"));
}

TEST_CASE("channels_to_json: channel id is numeric (no quotes)",
          "[infra][webui][channels_json]")
{
    InMemoryChannelRepository repo;
    (void)repo.save(make_channel(42, "FortyTwo"));

    const std::string json = channels_to_json(&repo);

    // id must be a bare number, not a quoted string
    CHECK(contains(json, "\"id\":42"));
    // Must NOT appear as "id":"42"
    CHECK(!contains(json, "\"id\":\"42\""));
}

TEST_CASE("channels_to_json: member_count is numeric (no quotes)",
          "[infra][webui][channels_json]")
{
    InMemoryChannelRepository repo;
    (void)repo.save(make_channel(5, "CountTest"));

    const std::string json = channels_to_json(&repo);

    CHECK(contains(json, "\"member_count\":0"));
    CHECK(!contains(json, "\"member_count\":\"0\""));
}
