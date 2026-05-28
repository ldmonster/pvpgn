// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file helpfile_source.hpp
/// Port: help-text lookup for the /help command.

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pvpgn::application::ports {

/// Port: help-file source interface for hexagonal architecture.
/// Implementations load help text from a flat file, database, or in-memory map.
class IHelpfileSource {
public:
    virtual ~IHelpfileSource() = default;

    /// Return the help text for `command_name`, or `std::nullopt` if no
    /// entry exists for that command.
    virtual std::optional<std::string>
    lookup(std::string_view command_name) const = 0;

    /// Return all command names that have registered help entries.
    virtual std::vector<std::string>
    all_commands() const = 0;
};

}  // namespace pvpgn::application::ports
