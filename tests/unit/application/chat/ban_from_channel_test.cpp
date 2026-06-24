// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::chat::BanFromChannel`. Exercises the use-case
// against the in-memory channel repository and a fake permission checker.
//
// Authority rule under test (mirrors original `_handle_ban_command`):
//   * the banner MUST hold the "operator" command group;
//   * an operator/admin target is immune and cannot be banned.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "application/chat/ban_from_channel.hpp"
#include "domain/chat/channel.hpp"
#include "domain/moderation/ports.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/user_name.hpp"
#include "channel_repository.hpp"

namespace {

using namespace pvpgn;
using application::chat::BanFromChannel;
using application::chat::BanFromChannelRequest;
using application::chat::BanFromChannelError;

domain::UserName make_name(std::string_view s) {
    auto r = domain::UserName::parse(s);
    if (!r) {
        auto fallback = domain::UserName::parse("Unknown");
        return fallback.value();
    }
    return r.value();
}

// ---------------------------------------------------------------------------
// Fake IPermissionChecker — grants named command groups per account.
// ---------------------------------------------------------------------------

class FakePermissionChecker final
    : public domain::moderation::IPermissionChecker {
public:
    void grant_group(domain::AccountId account, std::string group) {
        groups_[account.value()].push_back(std::move(group));
    }

    bool has_permission(domain::AccountId,
                        domain::moderation::Permission) const override {
        return false;
    }

    bool has_command_group(domain::AccountId account,
                           std::string_view group) const override {
        auto it = groups_.find(account.value());
        if (it == groups_.end()) return false;
        for (const auto& g : it->second) {
            if (g == group) return true;
        }
        return false;
    }

private:
    std::unordered_map<std::uint32_t, std::vector<std::string>> groups_;
};

struct Fixture {
    infra::storage::InMemoryChannelRepository channels;
    std::shared_ptr<FakePermissionChecker>    permissions =
        std::make_shared<FakePermissionChecker>();

    domain::AccountId alice_id{1};    // channel operator
    domain::AccountId bob_id{2};      // regular member
    domain::AccountId charlie_id{3};  // regular member
    domain::AccountId opal_id{4};     // operator target (immune)
    domain::AccountId adam_id{5};     // admin target (immune)
    domain::ChannelId channel_id{1};

    void setup_channel_with_members() {
        auto ch = domain::chat::Channel::create(
            channel_id, "TestChannel", domain::chat::ChannelPolicy{});
        auto star_tag = domain::ClientTag::parse("STAR").value();
        (void)ch.admit(alice_id, star_tag);
        (void)ch.admit(bob_id, star_tag);
        (void)ch.admit(charlie_id, star_tag);
        (void)ch.admit(opal_id, star_tag);
        (void)ch.admit(adam_id, star_tag);
        REQUIRE(channels.save(ch));

        permissions->grant_group(alice_id, "operator");
        permissions->grant_group(opal_id, "operator");
        permissions->grant_group(adam_id, "admin");
    }

    BanFromChannel make_use_case() {
        return BanFromChannel{
            std::shared_ptr<infra::storage::InMemoryChannelRepository>(
                &channels, [](auto*) {}),
            permissions,
            nullptr   // Message router not tested here
        };
    }
};

}  // namespace

TEST_CASE("BanFromChannel: operator banning a normal member succeeds",
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

TEST_CASE("BanFromChannel: non-operator ban is rejected as NotAuthorized",
          "[application][chat][ban]") {
    Fixture f;
    f.setup_channel_with_members();

    auto uc = f.make_use_case();
    // Bob is a normal member, not an operator.
    BanFromChannelRequest req{
        .banner_id = f.bob_id,
        .target_id = f.charlie_id,
        .target_name = make_name("Charlie"),
        .channel_id = f.channel_id,
        .reason = "priv-esc attempt",
    };
    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == BanFromChannelError::NotAuthorized);
}

TEST_CASE("BanFromChannel: banning an operator target is refused",
          "[application][chat][ban]") {
    Fixture f;
    f.setup_channel_with_members();

    auto uc = f.make_use_case();
    BanFromChannelRequest req{
        .banner_id = f.alice_id,
        .target_id = f.opal_id,   // operator — immune
        .target_name = make_name("Opal"),
        .channel_id = f.channel_id,
        .reason = "cannot ban operators",
    };
    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == BanFromChannelError::NotAuthorized);
}

TEST_CASE("BanFromChannel: banning an admin target is refused",
          "[application][chat][ban]") {
    Fixture f;
    f.setup_channel_with_members();

    auto uc = f.make_use_case();
    BanFromChannelRequest req{
        .banner_id = f.alice_id,
        .target_id = f.adam_id,   // admin — immune
        .target_name = make_name("Adam"),
        .channel_id = f.channel_id,
        .reason = "cannot ban administrators",
    };
    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == BanFromChannelError::NotAuthorized);
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

TEST_CASE("BanFromChannel: operator not a channel member fails membership",
          "[application][chat][ban]") {
    Fixture f;
    f.setup_channel_with_members();

    // Dana holds the operator group but is not in the channel.
    domain::AccountId dana_id{42};
    f.permissions->grant_group(dana_id, "operator");

    auto uc = f.make_use_case();
    BanFromChannelRequest req{
        .banner_id = dana_id,
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

    // First ban bob via the domain kick (adds to banlist).
    auto ch = f.channels.find_by_id(f.channel_id);
    if (ch) {
        auto channel = ch.value();
        channel.kick(f.alice_id, f.bob_id);
        REQUIRE(f.channels.save(channel));
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
