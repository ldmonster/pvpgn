// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file command_registry.hpp
/// Command dispatch registry — centralized command handler management.
///
/// Registers chat commands with their required permissions and handlers.
/// Dispatches command strings to appropriate handlers with permission checks.

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "application/ports/command_registry.hpp"
#include "application/ports/permission_checker.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::chat {

/// Signature for command handlers.
using CommandHandler = std::function<core::Result<std::string, core::Error>(
    domain::AccountId caller, const std::vector<std::string_view>& args)>;

class CommandRegistry : public application::ports::ICommandRegistry {
public:
    CommandRegistry() = default;

    /// Register a new command.
    void register_command(std::string_view name,
                          application::ports::Permission required_permission,
                          CommandHandler handler);

    /// Dispatch a command line, checking permissions.
    core::Result<std::string, core::Error>
    dispatch(domain::AccountId caller, std::string_view command_line,
             const application::ports::IPermissionChecker& checker) const override;

    /// List all commands available to the given account.
    std::vector<std::string> list_available(
        domain::AccountId caller,
        const application::ports::IPermissionChecker& checker) const override;

private:
    struct CommandEntry {
        application::ports::Permission required_permission;
        CommandHandler                 handler;
    };

    std::unordered_map<std::string, CommandEntry> commands_;

    // Parse a command line into name and arguments
    static bool parse_command_line(std::string_view line, std::string& name,
                                   std::vector<std::string_view>& args);
};

}  // namespace pvpgn::application::chat
