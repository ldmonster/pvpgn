// SPDX-License-Identifier: GPL-2.0-or-later
//
// Parse/edge-branch tests for CommandRegistry::dispatch.
// Targets the command-line parsing arms not reached by the happy-path
// suite in command_registry_test.cpp: whitespace-only input,
// leading-whitespace command names, and multi-arg tokenisation that the
// handler receives.

#include <catch2/catch_test_macros.hpp>

#include "application/chat/command_registry.hpp"
#include "infra/inmemory/in_memory_permission_checker.hpp"
#include "domain/shared/ids.hpp"
#include "domain/moderation/ports.hpp"

namespace {

using pvpgn::domain::AccountId;
using pvpgn::application::chat::CommandRegistry;
using pvpgn::application::chat::CommandHandler;
using pvpgn::domain::moderation::Permission;
using pvpgn::infra::inmemory::InMemoryPermissionChecker;

AccountId make_id(std::uint32_t v) { return AccountId{v}; }

/// A handler that joins its received args with '|' so tests can assert
/// exactly how the command line was tokenised.
CommandHandler args_handler() {
    return [](pvpgn::domain::AccountId,
              const std::vector<std::string_view>& args)
               -> pvpgn::core::Result<std::string, pvpgn::core::Error> {
        std::string out;
        for (std::size_t i = 0; i < args.size(); ++i) {
            if (i) out += '|';
            out += std::string(args[i]);
        }
        return pvpgn::core::Result<std::string, pvpgn::core::Error>(out);
    };
}

}  // namespace

TEST_CASE("CommandRegistry: whitespace-only line returns InvalidArgument",
          "[application][chat][command_registry]") {
    CommandRegistry reg;
    InMemoryPermissionChecker checker;
    const auto alice = make_id(1);

    auto result = reg.dispatch(alice, "   \t  ", checker);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::InvalidArgument);
}

TEST_CASE("CommandRegistry: leading whitespace before the command name is skipped",
          "[application][chat][command_registry]") {
    CommandRegistry reg;
    InMemoryPermissionChecker checker;
    const auto alice = make_id(1);

    reg.register_command("kick", Permission::KickUser, args_handler());
    checker.grant(alice, Permission::KickUser);

    // Leading spaces must not become part of the looked-up command name.
    auto result = reg.dispatch(alice, "   kick bob", checker);
    REQUIRE(result.has_value());
    CHECK(result.value() == "bob");
}

TEST_CASE("CommandRegistry: multiple args separated by runs of whitespace",
          "[application][chat][command_registry]") {
    CommandRegistry reg;
    InMemoryPermissionChecker checker;
    const auto alice = make_id(1);

    reg.register_command("op", Permission::KickUser, args_handler());
    checker.grant(alice, Permission::KickUser);

    auto result = reg.dispatch(alice, "op  alice   bob\tcarol", checker);
    REQUIRE(result.has_value());
    CHECK(result.value() == "alice|bob|carol");
}

TEST_CASE("CommandRegistry: command with trailing whitespace and no args",
          "[application][chat][command_registry]") {
    CommandRegistry reg;
    InMemoryPermissionChecker checker;
    const auto alice = make_id(1);

    reg.register_command("list", Permission::KickUser, args_handler());
    checker.grant(alice, Permission::KickUser);

    auto result = reg.dispatch(alice, "list   ", checker);
    REQUIRE(result.has_value());
    CHECK(result.value() == "");  // no args parsed
}

TEST_CASE("CommandRegistry: known-command lookup miss after a near-name registration",
          "[application][chat][command_registry]") {
    CommandRegistry reg;
    InMemoryPermissionChecker checker;
    const auto alice = make_id(1);

    reg.register_command("kick", Permission::KickUser, args_handler());
    checker.grant(alice, Permission::KickUser);

    // "kicks" is not "kick" -> map lookup miss -> NotFound.
    auto result = reg.dispatch(alice, "kicks bob", checker);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::NotFound);
}
