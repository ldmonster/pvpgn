// SPDX-License-Identifier: GPL-2.0-or-later
//
// Edge/error-branch tests for `application::chat::OpFromChannel`.
// Covers the lookup/validation arms that op_from_channel_test.cpp does
// not reach:
//   * channel not found (find_by_id miss)
//   * target name fails UserName::parse (invalid characters)
//   * target name parses but account does not exist
// The happy path, PermissionDenied and "target not in channel" arms are
// already covered by op_from_channel_test.cpp, so they are not repeated.

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "application/chat/op_from_channel.hpp"
#include "domain/identity/ports.hpp"
#include "domain/moderation/ports.hpp"
#include "domain/chat/channel.hpp"
#include "domain/identity/account.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"
#include "channel_repository.hpp"

namespace {

using namespace pvpgn;
using application::chat::OpFromChannel;
using application::chat::OpFromChannelCommand;
using application::chat::OpFromChannelError;

domain::identity::Account make_account(domain::AccountId id,
                                       std::string_view name) {
    auto uname = domain::UserName::parse(name);
    REQUIRE(uname);
    auto locale = domain::Locale::parse_or_default("enUS");
    return domain::identity::Account::rehydrate(
        id, uname.value(), domain::BNHash{}, locale,
        domain::identity::CommandGroupMask{}, std::nullopt, false);
}

// ---------------------------------------------------------------------------
// Fake IAccountRepository (mirrors op_from_channel_test.cpp)
// ---------------------------------------------------------------------------

class FakeAccountRepository final
    : public domain::identity::IAccountRepository {
public:
    void add(domain::identity::Account acct) {
        accounts_.emplace(std::string{acct.name().display()}, std::move(acct));
    }

    core::Result<domain::identity::Account>
    find_by_name(const domain::UserName& name) const override {
        auto it = accounts_.find(std::string{name.display()});
        if (it == accounts_.end()) {
            return core::fail(
                core::Error{core::StatusCode::NotFound, "account not found"});
        }
        return it->second;
    }

    core::Result<domain::identity::Account>
    find_by_id(domain::AccountId id) const override {
        for (auto& [_, a] : accounts_) {
            if (a.id() == id) return a;
        }
        return core::fail(
            core::Error{core::StatusCode::NotFound, "account not found"});
    }

    core::Status<>
    save(const domain::identity::Account&) override { return core::ok(); }

    core::Status<>
    remove(domain::AccountId id) override {
        for (auto it = accounts_.begin(); it != accounts_.end(); ++it) {
            if (it->second.id() == id) { accounts_.erase(it); break; }
        }
        return core::ok();
    }

    void forEach(
        std::function<bool(const domain::identity::Account&)> pred) const override {
        for (auto& [_, a] : accounts_) {
            if (!pred(a)) break;
        }
    }

    std::size_t size() const noexcept override { return accounts_.size(); }

private:
    std::unordered_map<std::string, domain::identity::Account> accounts_;
};

// ---------------------------------------------------------------------------
// Fake IPermissionChecker (mirrors op_from_channel_test.cpp)
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
    std::unordered_map<uint32_t, std::vector<std::string>> groups_;
};

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

struct Fixture {
    infra::storage::InMemoryChannelRepository channels;
    std::shared_ptr<FakeAccountRepository>    accounts =
        std::make_shared<FakeAccountRepository>();
    std::shared_ptr<FakePermissionChecker>    permissions =
        std::make_shared<FakePermissionChecker>();

    domain::AccountId alice_id{1};   // operator
    domain::AccountId bob_id{2};     // regular member
    domain::ChannelId channel_id{1};

    void setup() {
        accounts->add(make_account(alice_id, "Alice"));
        accounts->add(make_account(bob_id, "Bob"));

        auto ch = domain::chat::Channel::create(
            channel_id, "TestChannel", domain::chat::ChannelPolicy{});
        auto star_tag = domain::ClientTag::parse("STAR").value();
        (void)ch.admit(alice_id, star_tag);
        (void)ch.admit(bob_id, star_tag);
        REQUIRE(channels.save(ch));

        permissions->grant_group(alice_id, "operator");
    }

    OpFromChannel make_use_case() {
        return OpFromChannel{
            std::shared_ptr<infra::storage::InMemoryChannelRepository>(
                &channels, [](auto*) {}),
            accounts,
            permissions,
        };
    }
};

}  // namespace

TEST_CASE("OpFromChannel: unknown channel returns ChannelNotFound",
          "[application][chat][op_from_channel]") {
    Fixture f;
    f.setup();

    auto uc = f.make_use_case();
    OpFromChannelCommand cmd{
        .requester_id = f.alice_id,
        .channel_id   = domain::ChannelId{999},  // no such channel
        .target_name  = "Bob",
        .grant        = true,
    };
    auto r = uc.execute(cmd);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == OpFromChannelError::ChannelNotFound);
}

TEST_CASE("OpFromChannel: unparsable target name returns TargetNotFound",
          "[application][chat][op_from_channel]") {
    Fixture f;
    f.setup();

    auto uc = f.make_use_case();
    OpFromChannelCommand cmd{
        .requester_id = f.alice_id,
        .channel_id   = f.channel_id,
        .target_name  = "!!!",  // invalid UserName characters -> parse fails
        .grant        = true,
    };
    auto r = uc.execute(cmd);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == OpFromChannelError::TargetNotFound);
}

TEST_CASE("OpFromChannel: parseable name with no account returns TargetNotFound",
          "[application][chat][op_from_channel]") {
    Fixture f;
    f.setup();

    auto uc = f.make_use_case();
    OpFromChannelCommand cmd{
        .requester_id = f.alice_id,
        .channel_id   = f.channel_id,
        .target_name  = "Nobody",  // valid name, but no such account
        .grant        = true,
    };
    auto r = uc.execute(cmd);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == OpFromChannelError::TargetNotFound);
}
