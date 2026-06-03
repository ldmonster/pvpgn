// SPDX-License-Identifier: GPL-2.0-or-later
//
// Unit tests for application::auth::InMemoryPermissionChecker — the use-case
// that maps an account's command groups (1=admin, 2=mod, 3=operator, 4=voice)
// to moderation Permission sets, backed by an IAccountRepository.
//
// Distinct from tests/.../permission_checker_test.cpp, which covers the
// *infra* infra::inmemory::InMemoryPermissionChecker (a flat grant store).

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string_view>

#include "application/auth/permission_checker.hpp"
#include "domain/identity/account.hpp"
#include "domain/moderation/ports.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"
#include "infra/inmemory/account_repository.hpp"

namespace {

using namespace pvpgn;
using application::auth::InMemoryPermissionChecker;
using P = domain::moderation::Permission;

domain::UserName make_name(std::string_view s) {
    auto r = domain::UserName::parse(s);
    REQUIRE(r);
    return r.value();
}

// Build an account with the given command groups granted (1..8).
domain::identity::Account
make_account(domain::AccountId id, std::string_view name,
             std::initializer_list<std::uint8_t> groups) {
    domain::identity::CommandGroupMask mask;
    for (auto g : groups) mask.grant(g);
    return domain::identity::Account::rehydrate(
        id, make_name(name), domain::BNHash{},
        domain::Locale::parse_or_default("enUS"), mask, std::nullopt, false);
}

// A repository seeded with the given accounts, returned as a shared_ptr so the
// checker can share it.
std::shared_ptr<infra::inmemory::InMemoryAccountRepository>
repo_with(std::initializer_list<domain::identity::Account> accounts) {
    auto repo = std::make_shared<infra::inmemory::InMemoryAccountRepository>();
    for (const auto& a : accounts) REQUIRE(repo->save(a));
    return repo;
}

}  // namespace

TEST_CASE("app PermissionChecker: admin (group 1) has admin-only permissions",
          "[application][auth][permission_checker]") {
    auto repo = repo_with({make_account(domain::AccountId{1}, "Admin", {1})});
    InMemoryPermissionChecker checker{repo};

    CHECK(checker.has_permission(domain::AccountId{1}, P::ShutdownServer));
    CHECK(checker.has_permission(domain::AccountId{1}, P::BanUser));
    CHECK(checker.has_permission(domain::AccountId{1}, P::CreateChannel));
}

TEST_CASE("app PermissionChecker: mod (group 2) has moderation but not admin",
          "[application][auth][permission_checker]") {
    auto repo = repo_with({make_account(domain::AccountId{2}, "Mod", {2})});
    InMemoryPermissionChecker checker{repo};

    CHECK(checker.has_permission(domain::AccountId{2}, P::KickUser));
    CHECK(checker.has_permission(domain::AccountId{2}, P::SilenceUser));
    // Admin-only permissions must be denied to a mod.
    CHECK_FALSE(checker.has_permission(domain::AccountId{2}, P::ShutdownServer));
    CHECK_FALSE(checker.has_permission(domain::AccountId{2}, P::ReloadConfig));
}

TEST_CASE("app PermissionChecker: operator (group 3) manages channels only",
          "[application][auth][permission_checker]") {
    auto repo = repo_with({make_account(domain::AccountId{3}, "Op", {3})});
    InMemoryPermissionChecker checker{repo};

    CHECK(checker.has_permission(domain::AccountId{3}, P::CreateChannel));
    CHECK(checker.has_permission(domain::AccountId{3}, P::SetChannelTopic));
    CHECK_FALSE(checker.has_permission(domain::AccountId{3}, P::KickUser));
    CHECK_FALSE(checker.has_permission(domain::AccountId{3}, P::ShutdownServer));
}

TEST_CASE("app PermissionChecker: voice (group 4) has only SetChannelTopic",
          "[application][auth][permission_checker]") {
    auto repo = repo_with({make_account(domain::AccountId{4}, "Voice", {4})});
    InMemoryPermissionChecker checker{repo};

    CHECK(checker.has_permission(domain::AccountId{4}, P::SetChannelTopic));
    CHECK_FALSE(checker.has_permission(domain::AccountId{4}, P::CreateChannel));
    CHECK_FALSE(checker.has_permission(domain::AccountId{4}, P::KickUser));
}

TEST_CASE("app PermissionChecker: account with no groups has no permissions",
          "[application][auth][permission_checker]") {
    auto repo = repo_with({make_account(domain::AccountId{5}, "Plain", {})});
    InMemoryPermissionChecker checker{repo};

    CHECK_FALSE(checker.has_permission(domain::AccountId{5}, P::KickUser));
    CHECK_FALSE(checker.has_permission(domain::AccountId{5}, P::SetChannelTopic));
}

TEST_CASE("app PermissionChecker: unknown account is denied",
          "[application][auth][permission_checker]") {
    auto repo = repo_with({});  // empty
    InMemoryPermissionChecker checker{repo};

    CHECK_FALSE(checker.has_permission(domain::AccountId{99}, P::KickUser));
    CHECK_FALSE(checker.has_command_group(domain::AccountId{99}, "admin"));
}

TEST_CASE("app PermissionChecker: combined groups union their permissions",
          "[application][auth][permission_checker]") {
    // An account in both operator (3) and voice (4): gets the union.
    auto repo = repo_with({make_account(domain::AccountId{6}, "OpVoice", {3, 4})});
    InMemoryPermissionChecker checker{repo};

    CHECK(checker.has_permission(domain::AccountId{6}, P::CreateChannel));   // op
    CHECK(checker.has_permission(domain::AccountId{6}, P::SetChannelTopic)); // both
    CHECK_FALSE(checker.has_permission(domain::AccountId{6}, P::ShutdownServer));
}

TEST_CASE("app PermissionChecker: has_command_group reflects membership",
          "[application][auth][permission_checker]") {
    auto repo = repo_with({
        make_account(domain::AccountId{7}, "Admin2", {1}),
        make_account(domain::AccountId{8}, "Mod2", {2}),
    });
    InMemoryPermissionChecker checker{repo};

    CHECK(checker.has_command_group(domain::AccountId{7}, "admin"));
    CHECK_FALSE(checker.has_command_group(domain::AccountId{7}, "mod"));

    CHECK(checker.has_command_group(domain::AccountId{8}, "mod"));
    CHECK_FALSE(checker.has_command_group(domain::AccountId{8}, "admin"));
}
