// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/chat/command_registry.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace pvpgn::application::chat {

namespace {
// ASCII-lowercase a command name. The original server dispatches chat
// slash-commands case-INsensitively (command.cpp strstart -> strncasecmp),
// so command keys are normalized to lowercase both at registration and lookup.
std::string to_lower_ascii(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char ch : s) {
        out.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(ch))));
    }
    return out;
}
}  // namespace

void CommandRegistry::register_command(std::string_view name,
                                       domain::moderation::Permission required_permission,
                                       CommandHandler handler) {
    commands_[to_lower_ascii(name)] = {required_permission, handler};
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

    // Command-name lookup is case-insensitive (see to_lower_ascii note).
    name = to_lower_ascii(line.substr(start, end - start));

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
