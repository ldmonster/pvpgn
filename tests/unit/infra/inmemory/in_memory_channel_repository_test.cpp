// SPDX-License-Identifier: GPL-2.0-or-later
//
// InMemoryChannelRepository — channel id assignment.
//
// Regression guard for bug-hunt finding F-W16b: the repository used to store a
// channel verbatim under channel.id().value(). JoinChannel creates channels
// with the id-0 "assign on persist" sentinel, so EVERY new channel landed in
// by_id_[0] -- distinct channels collided (the last save won) and id 0 could not
// double as the FSM's "not in a channel" marker. The repository now allocates a
// fresh monotonic id (>= 1) for any channel saved with id 0.

#include <catch2/catch_test_macros.hpp>

#include "domain/chat/channel.hpp"
#include "domain/shared/ids.hpp"
#include "infra/inmemory/channel_repository.hpp"

using namespace pvpgn;
using pvpgn::infra::inmemory::InMemoryChannelRepository;

namespace {

domain::chat::Channel make_channel(const char* name) {
    return domain::chat::Channel::create(
        domain::ChannelId{0},  // id-0 sentinel: repo must assign
        name,
        domain::chat::ChannelPolicy{.flags = {}, .max_members = 0,
                                    .client = domain::ClientTag{}});
}

}  // namespace

TEST_CASE("InMemoryChannelRepository assigns a non-zero id on first save",
          "[infra][inmemory][channel]") {
    InMemoryChannelRepository repo;
    REQUIRE(repo.save(make_channel("Red")).has_value());

    auto red = repo.find_by_name("Red");
    REQUIRE(red.has_value());
    CHECK(red.value().id().value() != 0u);  // 0 stays reserved
}

TEST_CASE("InMemoryChannelRepository gives distinct channels distinct ids",
          "[infra][inmemory][channel]") {
    InMemoryChannelRepository repo;
    REQUIRE(repo.save(make_channel("Red")).has_value());
    REQUIRE(repo.save(make_channel("Blue")).has_value());

    auto red  = repo.find_by_name("Red");
    auto blue = repo.find_by_name("Blue");
    REQUIRE(red.has_value());
    REQUIRE(blue.has_value());

    // The bug: both collided in by_id_[0], so "Red" resolved to the Blue object.
    CHECK(red.value().id().value() != blue.value().id().value());
    CHECK(red.value().name() == "Red");
    CHECK(blue.value().name() == "Blue");

    // Both remain independently retrievable by their assigned ids.
    auto red_by_id  = repo.find_by_id(red.value().id());
    auto blue_by_id = repo.find_by_id(blue.value().id());
    REQUIRE(red_by_id.has_value());
    REQUIRE(blue_by_id.has_value());
    CHECK(red_by_id.value().name() == "Red");
    CHECK(blue_by_id.value().name() == "Blue");
    CHECK(repo.size() == 2u);
}

TEST_CASE("InMemoryChannelRepository preserves a non-zero id on re-save",
          "[infra][inmemory][channel]") {
    InMemoryChannelRepository repo;
    REQUIRE(repo.save(make_channel("Red")).has_value());
    auto red = repo.find_by_name("Red");
    REQUIRE(red.has_value());
    const auto assigned = red.value().id();

    // Re-saving the already-persisted channel (e.g. after a member joins) must
    // keep the same id, not allocate a new one.
    domain::chat::Channel updated = red.value();
    updated.admit(domain::AccountId{42}, domain::ClientTag{});
    REQUIRE(repo.save(updated).has_value());

    auto again = repo.find_by_name("Red");
    REQUIRE(again.has_value());
    CHECK(again.value().id().value() == assigned.value());
    CHECK(again.value().contains(domain::AccountId{42}));
    CHECK(repo.size() == 1u);
}
