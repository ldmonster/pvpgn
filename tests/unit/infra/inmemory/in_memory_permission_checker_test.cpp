// SPDX-License-Identifier: GPL-2.0-or-later

#include <catch2/catch_test_macros.hpp>

#include "infra/inmemory/in_memory_permission_checker.hpp"

namespace pvpgn::infra::inmemory {

using Perm = application::ports::Permission;

TEST_CASE("InMemoryPermissionChecker: HasPermissionFalseByDefault", "[infra][inmemory]") {
    InMemoryPermissionChecker checker;
    REQUIRE_FALSE(checker.has_permission(domain::AccountId{1}, Perm::BanUser));
}

TEST_CASE("InMemoryPermissionChecker: GrantAndCheckPermission", "[infra][inmemory]") {
    InMemoryPermissionChecker checker;
    const domain::AccountId account{10};

    checker.grant(account, Perm::KickUser);
    REQUIRE(checker.has_permission(account, Perm::KickUser));
}

TEST_CASE("InMemoryPermissionChecker: RevokePermission", "[infra][inmemory]") {
    InMemoryPermissionChecker checker;
    const domain::AccountId account{5};

    checker.grant(account, Perm::BanUser);
    REQUIRE(checker.has_permission(account, Perm::BanUser));

    checker.revoke(account, Perm::BanUser);
    REQUIRE_FALSE(checker.has_permission(account, Perm::BanUser));
}

TEST_CASE("InMemoryPermissionChecker: RevokeNonGrantedPermissionIsNoOp", "[infra][inmemory]") {
    InMemoryPermissionChecker checker;
    const domain::AccountId account{3};

    REQUIRE_NOTHROW(checker.revoke(account, Perm::ShutdownServer));
    REQUIRE_FALSE(checker.has_permission(account, Perm::ShutdownServer));
}

TEST_CASE("InMemoryPermissionChecker: MultiplePermissionsPerAccount", "[infra][inmemory]") {
    InMemoryPermissionChecker checker;
    const domain::AccountId account{20};

    checker.grant(account, Perm::CreateChannel);
    checker.grant(account, Perm::DeleteChannel);
    checker.grant(account, Perm::SetChannelTopic);

    REQUIRE(checker.has_permission(account, Perm::CreateChannel));
    REQUIRE(checker.has_permission(account, Perm::DeleteChannel));
    REQUIRE(checker.has_permission(account, Perm::SetChannelTopic));
    REQUIRE_FALSE(checker.has_permission(account, Perm::ShutdownServer));
}

TEST_CASE("InMemoryPermissionChecker: PermissionsAreIsolatedPerAccount", "[infra][inmemory]") {
    InMemoryPermissionChecker checker;
    const domain::AccountId admin{1};
    const domain::AccountId user{2};

    checker.grant(admin, Perm::ReloadConfig);

    REQUIRE(checker.has_permission(admin, Perm::ReloadConfig));
    REQUIRE_FALSE(checker.has_permission(user, Perm::ReloadConfig));
}

TEST_CASE("InMemoryPermissionChecker: HasCommandGroupFalseByDefault", "[infra][inmemory]") {
    InMemoryPermissionChecker checker;
    REQUIRE_FALSE(checker.has_command_group(domain::AccountId{1}, "admin"));
}

TEST_CASE("InMemoryPermissionChecker: GrantGroupAndCheck", "[infra][inmemory]") {
    InMemoryPermissionChecker checker;
    const domain::AccountId account{7};

    checker.grant_group(account, "admin");
    REQUIRE(checker.has_command_group(account, "admin"));
}

TEST_CASE("InMemoryPermissionChecker: RevokeGroup", "[infra][inmemory]") {
    InMemoryPermissionChecker checker;
    const domain::AccountId account{8};

    checker.grant_group(account, "mod");
    REQUIRE(checker.has_command_group(account, "mod"));

    checker.revoke_group(account, "mod");
    REQUIRE_FALSE(checker.has_command_group(account, "mod"));
}

TEST_CASE("InMemoryPermissionChecker: RevokeNonGrantedGroupIsNoOp", "[infra][inmemory]") {
    InMemoryPermissionChecker checker;
    const domain::AccountId account{9};

    REQUIRE_NOTHROW(checker.revoke_group(account, "operator"));
    REQUIRE_FALSE(checker.has_command_group(account, "operator"));
}

TEST_CASE("InMemoryPermissionChecker: MultipleGroupsPerAccount", "[infra][inmemory]") {
    InMemoryPermissionChecker checker;
    const domain::AccountId account{15};

    checker.grant_group(account, "admin");
    checker.grant_group(account, "mod");
    checker.grant_group(account, "voice");

    REQUIRE(checker.has_command_group(account, "admin"));
    REQUIRE(checker.has_command_group(account, "mod"));
    REQUIRE(checker.has_command_group(account, "voice"));
    REQUIRE_FALSE(checker.has_command_group(account, "operator"));
}

TEST_CASE("InMemoryPermissionChecker: GroupsAreIsolatedPerAccount", "[infra][inmemory]") {
    InMemoryPermissionChecker checker;
    const domain::AccountId a1{100};
    const domain::AccountId a2{200};

    checker.grant_group(a1, "admin");

    REQUIRE(checker.has_command_group(a1, "admin"));
    REQUIRE_FALSE(checker.has_command_group(a2, "admin"));
}

TEST_CASE("InMemoryPermissionChecker: GrantDuplicatePermissionIsIdempotent", "[infra][inmemory]") {
    InMemoryPermissionChecker checker;
    const domain::AccountId account{50};

    checker.grant(account, Perm::ViewLogs);
    checker.grant(account, Perm::ViewLogs);

    REQUIRE(checker.has_permission(account, Perm::ViewLogs));
    checker.revoke(account, Perm::ViewLogs);
    REQUIRE_FALSE(checker.has_permission(account, Perm::ViewLogs));
}

}  // namespace pvpgn::infra::inmemory
