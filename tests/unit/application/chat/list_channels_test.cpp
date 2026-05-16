// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::chat::ListChannels`. Exercises the use-case
// against the in-memory port adapters.

#include <catch2/catch_test_macros.hpp>

#include "application/chat/list_channels.hpp"
#include "domain/chat/channel.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "channel_repository.hpp"

namespace {

using namespace pvpgn;
using application::chat::ListChannels;
using application::chat::ListChannelsRequest;

domain::ClientTag make_tag(std::string_view s) {
    auto r = domain::ClientTag::parse(s);
    if (!r) return domain::ClientTag{};
    return r.value();
}

struct Fixture {
    infra::storage::InMemoryChannelRepository channels;
    domain::ChannelId                         channel_id_1{1};
    domain::ChannelId                         channel_id_2{2};

    void setup_channels() {
        auto ch1 = domain::chat::Channel::create(
            channel_id_1, "PublicChannel",
            domain::chat::ChannelPolicy{});
        REQUIRE(channels.save(ch1));

        domain::chat::ChannelPolicy policy;
        policy.client = make_tag("STAR");
        auto ch2 = domain::chat::Channel::create(
            channel_id_2, "StarChannel", policy);
        REQUIRE(channels.save(ch2));
    }

    ListChannels make_use_case() {
        return ListChannels{
            std::shared_ptr<infra::storage::InMemoryChannelRepository>(
                &channels, [](auto*) {})};
    }
};

}  // namespace

TEST_CASE("ListChannels: lists all channels when no filter",
          "[application][chat][list]") {
    Fixture f;
    f.setup_channels();

    auto uc = f.make_use_case();
    ListChannelsRequest req;
    auto r = uc.execute(req);

    REQUIRE(r);
    REQUIRE(r.value().size() == 2);
}

TEST_CASE("ListChannels: filters by client tag when provided",
          "[application][chat][list]") {
    Fixture f;
    f.setup_channels();

    auto uc = f.make_use_case();
    ListChannelsRequest req{
        .filter_by_tag = make_tag("STAR"),
        .max_results = 0,
    };
    auto r = uc.execute(req);

    REQUIRE(r);
    REQUIRE(r.value().size() == 1);
    REQUIRE(r.value()[0].name == "StarChannel");
}

TEST_CASE("ListChannels: respects max_results limit",
          "[application][chat][list]") {
    Fixture f;
    f.setup_channels();

    auto uc = f.make_use_case();
    ListChannelsRequest req{
        .filter_by_tag = domain::ClientTag{},
        .max_results = 1,
    };
    auto r = uc.execute(req);

    REQUIRE(r);
    REQUIRE(r.value().size() == 1);
}

TEST_CASE("ListChannels: returns channel info with correct fields",
          "[application][chat][list]") {
    Fixture f;
    f.setup_channels();

    auto uc = f.make_use_case();
    ListChannelsRequest req;
    auto r = uc.execute(req);

    REQUIRE(r);
    REQUIRE(r.value()[0].id == f.channel_id_1);
    REQUIRE(r.value()[0].name == "PublicChannel");
    REQUIRE(r.value()[0].member_count == 0);
}

TEST_CASE("ListChannels: invalid max_results returns error",
          "[application][chat][list]") {
    Fixture f;
    f.setup_channels();

    auto uc = f.make_use_case();
    ListChannelsRequest req{
        .filter_by_tag = domain::ClientTag{},
        .max_results = 0,
    };
    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
}
