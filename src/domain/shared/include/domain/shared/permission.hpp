// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file permission.hpp
/// Published-kernel authorization vocabulary: the `Permission` capability enum
/// and the `IPermissionChecker` port. Authorization is cross-cutting — the
/// `chat` context (command registry) and the `moderation` context both need it,
/// so it lives in the shared kernel rather than coupling chat to moderation
/// internals.

#include <cstdint>
#include <string_view>

#include "domain/shared/ids.hpp"

namespace pvpgn::domain {

/// A single authorizable capability.
///
/// Values are stable — do not reorder or remove entries; only append.
enum class Permission : std::uint16_t {
    // Channel management
    CreateChannel    = 0,
    DeleteChannel    = 1,
    SetChannelTopic  = 2,
    KickFromChannel  = 3,
    BanFromChannel   = 4,
    UnbanFromChannel = 5,

    // User moderation
    KickUser         = 6,
    BanUser          = 7,
    UnbanUser        = 8,
    BanIp            = 9,
    UnbanIp          = 10,
    SilenceUser      = 11,
    UnsilenceUser    = 12,

    // Clan management
    CreateClan       = 13,
    DisbandClan      = 14,

    // Admin views
    ViewAdminPanel   = 15,
    ViewBanList      = 16,
    ViewUserList     = 17,
    ViewGameList     = 18,

    // Server operations
    ShutdownServer   = 19,
    ReloadConfig     = 20,
    ViewLogs         = 21,
};

/// Port: check whether an account holds a given permission or command group.
class IPermissionChecker {
public:
    virtual ~IPermissionChecker() = default;

    /// Returns true if `account` has been granted `perm`.
    [[nodiscard]] virtual bool
    has_permission(domain::AccountId account, Permission perm) const = 0;

    /// Returns true if `account` belongs to the named command group.
    [[nodiscard]] virtual bool
    has_command_group(domain::AccountId account,
                      std::string_view group) const = 0;
};

}  // namespace pvpgn::domain
