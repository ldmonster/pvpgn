// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file helpfile_source.hpp
/// Port for `/help <cmd>` text lookups.

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pvpgn::application::ports {

class IHelpfileSource {
public:
    virtual ~IHelpfileSource() = default;

    /// Return the help text for `command_name`, or `std::nullopt` if the
    /// command has no registered help entry.
    [[nodiscard]] virtual std::optional<std::string>
        lookup(std::string_view command_name) const = 0;

    /// Return all commands that have registered help entries.
    [[nodiscard]] virtual std::vector<std::string>
        all_commands() const = 0;
};

} // namespace pvpgn::application::ports
