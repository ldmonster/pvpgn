// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file legacy_help_command_permissions.hpp
/// `IHelpCommandPermissions` adapter delegating to legacy
/// `command_get_group` + `account_get_command_groups`.

#include "application/admin_commands/help_command_permissions.hpp"

namespace pvpgn::integration::legacy_bnetd {

class LegacyHelpCommandPermissions final
    : public application::admin_commands::IHelpCommandPermissions {
public:
    bool is_visible(void* connection,
                    std::string_view canonical_name) const override;
};

}  // namespace pvpgn::integration::legacy_bnetd
