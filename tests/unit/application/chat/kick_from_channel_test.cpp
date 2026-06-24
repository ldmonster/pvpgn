// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::chat::KickFromChannel`. Exercises the use-case
// against the in-memory channel repository and a fake permission checker.
//
// Authority rule under test (mirrors original `_handle_kick_command`):
//   * the kicker MUST hold the "operator" command group;
//   * an operator/admin target is immune and cannot be kicked.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "application/chat/kick_from_channel.hpp"
#include "domain/chat/channel.hpp"
#include "domain/moderation/ports.hpp"
#include "domain/shared/ids.hpp"
#include "channel_repository.hpp"

namespace {

using namespace pvpgn;
using application::chat::KickFromChannel;
using application::chat::KickFromChannelRequest;
using application::chat::KickFromChannelError;

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

        // Alice is the only channel operator. Opal/Adam are privileged
        // targets that must be immune from being kicked.
        permissions->grant_group(alice_id, "operator");
        permissions->grant_group(opal_id, "operator");
        permissions->grant_group(adam_id, "admin");
    }

    KickFromChannel make_use_case() {
        return KickFromChannel{
            std::shared_ptr<infra::storage::InMemoryChannelRepository>(
                &channels, [](auto*) {}),
            permissions,
            nullptr   // Message router not tested here
        };
    }
};

}  // namespace

TEST_CASE("KickFromChannel: operator kicking a normal member succeeds",
          "[application][chat][kick]") {
    Fixture f;
    f.setup_channel_with_members();

    auto uc = f.make_use_case();
    KickFromChannelRequest req{
        .kicker_id = f.alice_id,
        .target_id = f.bob_id,
        .channel_id = f.channel_id,
        .reason = "spam",
    };
    auto r = uc.execute(req);

    REQUIRE(r);
}

TEST_CASE("KickFromChannel: non-operator kick is rejected as NotAuthorized",
          "[application][chat][kick]") {
    Fixture f;
    f.setup_channel_with_members();

    auto uc = f.make_use_case();
    // Bob is a normal member, not an operator.
    KickFromChannelRequest req{
        .kicker_id = f.bob_id,
        .target_id = f.charlie_id,
        .channel_id = f.channel_id,
        .reason = "priv-esc attempt",
    };
    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == KickFromChannelError::NotAuthorized);
}

TEST_CASE("KickFromChannel: kicking an operator target is refused",
          "[application][chat][kick]") {
    Fixture f;
    f.setup_channel_with_members();

    auto uc = f.make_use_case();
    KickFromChannelRequest req{
        .kicker_id = f.alice_id,
        .target_id = f.opal_id,   // operator — immune
        .channel_id = f.channel_id,
        .reason = "cannot kick operators",
    };
    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == KickFromChannelError::NotAuthorized);
}

TEST_CASE("KickFromChannel: kicking an admin target is refused",
          "[application][chat][kick]") {
    Fixture f;
    f.setup_channel_with_members();

    auto uc = f.make_use_case();
    KickFromChannelRequest req{
        .kicker_id = f.alice_id,
        .target_id = f.adam_id,   // admin — immune
        .channel_id = f.channel_id,
        .reason = "cannot kick administrators",
    };
    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == KickFromChannelError::NotAuthorized);
}

TEST_CASE("KickFromChannel: cannot kick self",
          "[application][chat][kick]") {
    Fixture f;
    f.setup_channel_with_members();

    auto uc = f.make_use_case();
    KickFromChannelRequest req{
        .kicker_id = f.alice_id,
        .target_id = f.alice_id,
        .channel_id = f.channel_id,
        .reason = "whatever",
    };
    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == KickFromChannelError::CannotKickSelf);
}

TEST_CASE("KickFromChannel: fails when target not in channel",
          "[application][chat][kick]") {
    Fixture f;
    f.setup_channel_with_members();

    auto uc = f.make_use_case();
    KickFromChannelRequest req{
        .kicker_id = f.alice_id,
        .target_id = domain::AccountId{999},
        .channel_id = f.channel_id,
        .reason = "not_here",
    };
    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == KickFromChannelError::TargetNotInChannel);
}

TEST_CASE("KickFromChannel: fails when channel not found",
          "[application][chat][kick]") {
    Fixture f;
    f.setup_channel_with_members();

    auto uc = f.make_use_case();
    KickFromChannelRequest req{
        .kicker_id = f.alice_id,
        .target_id = f.bob_id,
        .channel_id = domain::ChannelId{999},
        .reason = "gone",
    };
    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == KickFromChannelError::ChannelNotFound);
}

TEST_CASE("KickFromChannel: operator not a channel member fails membership",
          "[application][chat][kick]") {
    Fixture f;
    f.setup_channel_with_members();

    // Dana holds the operator group but is not in the channel.
    domain::AccountId dana_id{42};
    f.permissions->grant_group(dana_id, "operator");

    auto uc = f.make_use_case();
    KickFromChannelRequest req{
        .kicker_id = dana_id,
        .target_id = f.bob_id,
        .channel_id = f.channel_id,
        .reason = "not_member",
    };
    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == KickFromChannelError::TargetNotInChannel);
}
