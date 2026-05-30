// SPDX-License-Identifier: GPL-2.0-or-later
//
// Unit tests for InMemoryPermissionChecker (infra::inmemory variant).
//
// The application::auth::InMemoryPermissionChecker is backed by an
// IAccountRepository and maps command_groups to Permission sets.
// For unit tests we use the simpler infra::inmemory::InMemoryPermissionChecker
// which accepts explicit grant()/revoke() calls — no account repo needed.

#include <catch2/catch_test_macros.hpp>

#include "infra/inmemory/in_memory_permission_checker.hpp"
#include "domain/shared/ids.hpp"

namespace {

using pvpgn::domain::AccountId;
using pvpgn::infra::inmemory::InMemoryPermissionChecker;
using pvpgn::application::ports::Permission;

AccountId make_id(std::uint32_t v) { return AccountId{v}; }

}  // namespace

TEST_CASE("InMemoryPermissionChecker: no permissions by default",
          "[application][auth][permission_checker]") {
    InMemoryPermissionChecker checker;
    const auto alice = make_id(1);
    CHECK_FALSE(checker.has_permission(alice, Permission::KickUser));
    CHECK_FALSE(checker.has_permission(alice, Permission::BanUser));
    CHECK_FALSE(checker.has_command_group(alice, "admin"));
}

TEST_CASE("InMemoryPermissionChecker: grant and has_permission",
          "[application][auth][permission_checker]") {
    InMemoryPermissionChecker checker;
    const auto alice = make_id(1);

    checker.grant(alice, Permission::KickUser);
    CHECK(checker.has_permission(alice, Permission::KickUser));
    CHECK_FALSE(checker.has_permission(alice, Permission::BanUser));
}

TEST_CASE("InMemoryPermissionChecker: revoke removes permission",
          "[application][auth][permission_checker]") {
    InMemoryPermissionChecker checker;
    const auto alice = make_id(1);

    checker.grant(alice, Permission::SilenceUser);
    REQUIRE(checker.has_permission(alice, Permission::SilenceUser));

    checker.revoke(alice, Permission::SilenceUser);
    CHECK_FALSE(checker.has_permission(alice, Permission::SilenceUser));
}

TEST_CASE("InMemoryPermissionChecker: grant_group and has_command_group",
          "[application][auth][permission_checker]") {
    InMemoryPermissionChecker checker;
    const auto bob = make_id(2);

    checker.grant_group(bob, "admin");
    CHECK(checker.has_command_group(bob, "admin"));
    CHECK_FALSE(checker.has_command_group(bob, "mod"));
}

TEST_CASE("InMemoryPermissionChecker: revoke_group removes group",
          "[application][auth][permission_checker]") {
    InMemoryPermissionChecker checker;
    const auto bob = make_id(2);

    checker.grant_group(bob, "mod");
    REQUIRE(checker.has_command_group(bob, "mod"));

    checker.revoke_group(bob, "mod");
    CHECK_FALSE(checker.has_command_group(bob, "mod"));
}

TEST_CASE("InMemoryPermissionChecker: permissions are per-account",
          "[application][auth][permission_checker]") {
    InMemoryPermissionChecker checker;
    const auto alice = make_id(1);
    const auto bob   = make_id(2);

    checker.grant(alice, Permission::ViewUserList);
    CHECK(checker.has_permission(alice, Permission::ViewUserList));
    CHECK_FALSE(checker.has_permission(bob, Permission::ViewUserList));
}

TEST_CASE("InMemoryPermissionChecker: multiple permissions can be granted",
          "[application][auth][permission_checker]") {
    InMemoryPermissionChecker checker;
    const auto alice = make_id(1);

    checker.grant(alice, Permission::KickUser);
    checker.grant(alice, Permission::BanUser);
    checker.grant(alice, Permission::ViewBanList);

    CHECK(checker.has_permission(alice, Permission::KickUser));
    CHECK(checker.has_permission(alice, Permission::BanUser));
    CHECK(checker.has_permission(alice, Permission::ViewBanList));
    CHECK_FALSE(checker.has_permission(alice, Permission::ShutdownServer));
}

TEST_CASE("InMemoryPermissionChecker: revoke non-existent permission is no-op",
          "[application][auth][permission_checker]") {
    InMemoryPermissionChecker checker;
    const auto alice = make_id(1);
    // Should not throw or crash
    checker.revoke(alice, Permission::KickUser);
    CHECK_FALSE(checker.has_permission(alice, Permission::KickUser));
}
