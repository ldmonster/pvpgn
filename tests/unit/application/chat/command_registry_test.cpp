// SPDX-License-Identifier: GPL-2.0-or-later
//
// Unit tests for CommandRegistry: register, dispatch, list_available.

#include <catch2/catch_test_macros.hpp>

#include "application/chat/command_registry.hpp"
#include "infra/inmemory/in_memory_permission_checker.hpp"
#include "domain/shared/ids.hpp"

namespace {

using pvpgn::domain::AccountId;
using pvpgn::application::chat::CommandRegistry;
using pvpgn::application::chat::CommandHandler;
using pvpgn::application::ports::Permission;
using pvpgn::infra::inmemory::InMemoryPermissionChecker;

AccountId make_id(std::uint32_t v) { return AccountId{v}; }

/// A handler that always succeeds and returns its name.
CommandHandler echo_handler(std::string label) {
    return [label](pvpgn::domain::AccountId, const std::vector<std::string_view>&)
               -> pvpgn::core::Result<std::string, pvpgn::core::Error> {
        return pvpgn::core::Result<std::string, pvpgn::core::Error>(label);
    };
}

}  // namespace

TEST_CASE("CommandRegistry: dispatch unknown command returns NotFound",
          "[application][chat][command_registry]") {
    CommandRegistry reg;
    InMemoryPermissionChecker checker;
    const auto alice = make_id(1);

    auto result = reg.dispatch(alice, "kick bob", checker);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::NotFound);
}

TEST_CASE("CommandRegistry: dispatch empty line returns InvalidArgument",
          "[application][chat][command_registry]") {
    CommandRegistry reg;
    InMemoryPermissionChecker checker;
    const auto alice = make_id(1);

    auto result = reg.dispatch(alice, "", checker);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::InvalidArgument);
}

TEST_CASE("CommandRegistry: dispatch without permission returns PermissionDenied",
          "[application][chat][command_registry]") {
    CommandRegistry reg;
    InMemoryPermissionChecker checker;
    const auto alice = make_id(1);

    reg.register_command("kick", Permission::KickUser, echo_handler("kick"));

    // alice has no permissions
    auto result = reg.dispatch(alice, "kick bob", checker);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::PermissionDenied);
}

TEST_CASE("CommandRegistry: dispatch with permission succeeds",
          "[application][chat][command_registry]") {
    CommandRegistry reg;
    InMemoryPermissionChecker checker;
    const auto alice = make_id(1);

    reg.register_command("kick", Permission::KickUser, echo_handler("kick"));
    checker.grant(alice, Permission::KickUser);

    auto result = reg.dispatch(alice, "kick bob", checker);
    REQUIRE(result.has_value());
    CHECK(result.value() == "kick");
}

TEST_CASE("CommandRegistry: list_available returns only permitted commands",
          "[application][chat][command_registry]") {
    CommandRegistry reg;
    InMemoryPermissionChecker checker;
    const auto alice = make_id(1);

    reg.register_command("kick",     Permission::KickUser,    echo_handler("kick"));
    reg.register_command("ban",      Permission::BanUser,     echo_handler("ban"));
    reg.register_command("shutdown", Permission::ShutdownServer, echo_handler("shutdown"));

    checker.grant(alice, Permission::KickUser);
    checker.grant(alice, Permission::BanUser);
    // alice does NOT have ShutdownServer

    auto available = reg.list_available(alice, checker);
    REQUIRE(available.size() == 2);
    // list_available returns sorted results
    CHECK(available[0] == "ban");
    CHECK(available[1] == "kick");
}

TEST_CASE("CommandRegistry: list_available returns empty when no permissions",
          "[application][chat][command_registry]") {
    CommandRegistry reg;
    InMemoryPermissionChecker checker;
    const auto alice = make_id(1);

    reg.register_command("kick", Permission::KickUser, echo_handler("kick"));

    auto available = reg.list_available(alice, checker);
    CHECK(available.empty());
}

TEST_CASE("CommandRegistry: register_command overwrites existing entry",
          "[application][chat][command_registry]") {
    CommandRegistry reg;
    InMemoryPermissionChecker checker;
    const auto alice = make_id(1);

    reg.register_command("test", Permission::KickUser, echo_handler("first"));
    reg.register_command("test", Permission::KickUser, echo_handler("second"));

    checker.grant(alice, Permission::KickUser);
    auto result = reg.dispatch(alice, "test", checker);
    REQUIRE(result.has_value());
    CHECK(result.value() == "second");
}
