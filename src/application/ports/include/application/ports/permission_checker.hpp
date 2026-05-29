// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file permission_checker.hpp
/// Port for permission/authorization checking.
///
/// Determines whether an account has a specific permission or
/// belongs to a command group (e.g., "admin", "mod", "operator").

#include <string_view>

#include "domain/shared/ids.hpp"

namespace pvpgn::application::ports {

/// Fine-grained permissions for moderation and admin actions.
enum class Permission : std::uint16_t {
    // Chat/Channel operations
    CreateChannel,
    DeleteChannel,
    SetChannelTopic,
    KickFromChannel,
    BanFromChannel,
    UnbanFromChannel,

    // User moderation
    KickUser,
    BanUser,
    UnbanUser,
    BanIp,
    UnbanIp,
    SilenceUser,
    UnsilenceUser,

    // Clan operations
    CreateClan,
    DisbandClan,

    // Admin panel
    ViewAdminPanel,
    ViewBanList,
    ViewUserList,
    ViewGameList,

    // Server operations
    ShutdownServer,
    ReloadConfig,
    ViewLogs,
};

class IPermissionChecker {
public:
    virtual ~IPermissionChecker() = default;

    /// Check if an account has a specific permission.
    virtual bool has_permission(domain::AccountId account, Permission perm) const = 0;

    /// Check if an account belongs to a command group.
    /// Common groups: "admin", "mod", "operator", "voice"
    virtual bool has_command_group(domain::AccountId account,
                                   std::string_view group) const = 0;
};

}  // namespace pvpgn::application::ports
