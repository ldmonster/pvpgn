// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::chat::BanFromChannel`. Exercises the use-case
// against the in-memory port adapters.

#include <catch2/catch_test_macros.hpp>

#include "application/chat/ban_from_channel.hpp"
#include "domain/chat/channel.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/user_name.hpp"
#include "infra/storage/repository/channel_repository.hpp"

namespace {

using namespace pvpgn;
using application::chat::BanFromChannel;
using application::chat::BanFromChannelRequest;
using application::chat::BanFromChannelError;

domain::UserName make_name(std::string_view s) {
    auto r = domain::UserName::parse(s);
    if (!r) return domain::UserName{};
    return r.value();
}

struct Fixture {
    infra::storage::InMemoryChannelRepository channels;
    domain::AccountId                         alice_id{1};
    domain::AccountId                         bob_id{2};
    domain::ChannelId                         channel_id{1};

    void setup_channel_with_members() {
        auto ch = domain::chat::Channel::create(
            channel_id, "TestChannel", domain::chat::ChannelPolicy{});
        ch.admit(alice_id, domain::ClientTag{});
        ch.admit(bob_id, domain::ClientTag{});
        channels.save(ch);
    }

    BanFromChannel make_use_case() {
        return BanFromChannel{
            std::make_shared<infra::storage::InMemoryChannelRepository>(
                channels),
            nullptr,  // Account repository not tested here
            nullptr   // Message router not tested here
        };
    }
};

}  // namespace

TEST_CASE("BanFromChannel: banning a member succeeds",
          "[application][chat][ban]") {
    Fixture f;
    f.setup_channel_with_members();

    auto uc = f.make_use_case();
    BanFromChannelRequest req{
        .banner_id = f.alice_id,
        .target_id = f.bob_id,
        .target_name = make_name("Bob"),
        .channel_id = f.channel_id,
        .reason = "spam",
    };
    auto r = uc.execute(req);

    REQUIRE(r);
}

TEST_CASE("BanFromChannel: cannot ban self",
          "[application][chat][ban]") {
    Fixture f;
    f.setup_channel_with_members();

    auto uc = f.make_use_case();
    BanFromChannelRequest req{
        .banner_id = f.alice_id,
        .target_id = f.alice_id,
        .target_name = make_name("Alice"),
        .channel_id = f.channel_id,
        .reason = "whatever",
    };
    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == BanFromChannelError::CannotBanSelf);
}

TEST_CASE("BanFromChannel: fails when channel not found",
          "[application][chat][ban]") {
    Fixture f;
    f.setup_channel_with_members();

    auto uc = f.make_use_case();
    BanFromChannelRequest req{
        .banner_id = f.alice_id,
        .target_id = f.bob_id,
        .target_name = make_name("Bob"),
        .channel_id = domain::ChannelId{999},
        .reason = "gone",
    };
    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == BanFromChannelError::ChannelNotFound);
}

TEST_CASE("BanFromChannel: fails when banner not in channel",
          "[application][chat][ban]") {
    Fixture f;
    f.setup_channel_with_members();

    auto uc = f.make_use_case();
    BanFromChannelRequest req{
        .banner_id = domain::AccountId{999},
        .target_id = f.bob_id,
        .target_name = make_name("Bob"),
        .channel_id = f.channel_id,
        .reason = "not_member",
    };
    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == BanFromChannelError::InsufficientPermissions);
}

TEST_CASE("BanFromChannel: fails when target already banned",
          "[application][chat][ban]") {
    Fixture f;
    f.setup_channel_with_members();

    // First ban bob
    auto ch = f.channels.find_by_id(f.channel_id);
    if (ch) {
        auto channel = ch.value();
        channel.kick(f.alice_id, f.bob_id);  // This adds to banlist
        f.channels.save(channel);
    }

    auto uc = f.make_use_case();
    BanFromChannelRequest req{
        .banner_id = f.alice_id,
        .target_id = f.bob_id,
        .target_name = make_name("Bob"),
        .channel_id = f.channel_id,
        .reason = "already_banned",
    };
    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == BanFromChannelError::TargetAlreadyBanned);
}
