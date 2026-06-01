// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/chat/command_registry.hpp"

#include <algorithm>
#include <sstream>

namespace pvpgn::application::chat {

void CommandRegistry::register_command(std::string_view name,
                                       domain::moderation::Permission required_permission,
                                       CommandHandler handler) {
    commands_[std::string(name)] = {required_permission, handler};
}

core::Result<std::string, core::Error>
CommandRegistry::dispatch(domain::AccountId caller, std::string_view command_line,
                          const domain::moderation::IPermissionChecker& checker) const {
    std::string name;
    std::vector<std::string_view> args;

    if (!parse_command_line(command_line, name, args)) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument, "invalid command format"});
    }

    // Look up command
    auto it = commands_.find(name);
    if (it == commands_.end()) {
        return core::fail(core::Error{
            core::StatusCode::NotFound, "command not found"});
    }

    const auto& entry = it->second;

    // Check permission
    if (!checker.has_permission(caller, entry.required_permission)) {
        return core::fail(core::Error{
            core::StatusCode::PermissionDenied, "insufficient permissions"});
    }

    // Execute handler
    return entry.handler(caller, args);
}

std::vector<std::string> CommandRegistry::list_available(
    domain::AccountId caller,
    const domain::moderation::IPermissionChecker& checker) const {
    std::vector<std::string> result;

    for (const auto& [name, entry] : commands_) {
        if (checker.has_permission(caller, entry.required_permission)) {
            result.push_back(name);
        }
    }

    // Sort for consistent output
    std::sort(result.begin(), result.end());

    return result;
}

bool CommandRegistry::parse_command_line(std::string_view line, std::string& name,
                                         std::vector<std::string_view>& args) {
    if (line.empty()) {
        return false;
    }

    // Skip leading whitespace
    std::size_t start = 0;
    while (start < line.size() && std::isspace(line[start])) {
        ++start;
    }

    if (start >= line.size()) {
        return false;
    }

    // Extract command name
    std::size_t end = start;
    while (end < line.size() && !std::isspace(line[end])) {
        ++end;
    }

    name = std::string(line.substr(start, end - start));

    // Extract arguments
    while (end < line.size()) {
        // Skip whitespace
        while (end < line.size() && std::isspace(line[end])) {
            ++end;
        }

        if (end >= line.size()) {
            break;
        }

        // Find argument end
        std::size_t arg_start = end;
        while (end < line.size() && !std::isspace(line[end])) {
            ++end;
        }

        args.push_back(line.substr(arg_start, end - arg_start));
    }

    return !name.empty();
}

}  // namespace pvpgn::application::chat
