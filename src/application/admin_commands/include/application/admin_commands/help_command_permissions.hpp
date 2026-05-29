// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file help_command_permissions.hpp
/// Port answering the question "is `<command>` visible to this user
/// when listing available chat commands via `/help`?".
///
/// Replaces the legacy expression
/// `command_get_group(name) & account_get_command_groups(conn_get_account(c))`
/// found inline in `helpfile.cpp::list_commands`.
///
/// The connection is opaque so this header is legacy-free.

#include <string_view>

namespace pvpgn::application::admin_commands {

class IHelpCommandPermissions {
public:
    virtual ~IHelpCommandPermissions() = default;

    /// @return true if the connection is allowed to see the named
    ///         command in `/help` output.
    /// @param canonical_name  command name including its `/` prefix
    ///                        (e.g. `"/help"`).
    [[nodiscard]] virtual bool is_visible(void* connection,
                                          std::string_view canonical_name) const = 0;
};

}  // namespace pvpgn::application::admin_commands
