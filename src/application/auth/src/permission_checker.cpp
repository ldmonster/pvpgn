// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/auth/permission_checker.hpp"

#include <algorithm>

#include "domain/identity/ports.hpp"

namespace pvpgn::application::auth {

void InMemoryPermissionChecker::init_group_mappings() {
    using P = application::ports::Permission;

    // Admin group: all permissions
    auto& admin_perms = group_permissions_["admin"];
    admin_perms.insert(static_cast<std::uint16_t>(P::CreateChannel));
    admin_perms.insert(static_cast<std::uint16_t>(P::DeleteChannel));
    admin_perms.insert(static_cast<std::uint16_t>(P::SetChannelTopic));
    admin_perms.insert(static_cast<std::uint16_t>(P::KickFromChannel));
    admin_perms.insert(static_cast<std::uint16_t>(P::BanFromChannel));
    admin_perms.insert(static_cast<std::uint16_t>(P::UnbanFromChannel));
    admin_perms.insert(static_cast<std::uint16_t>(P::KickUser));
    admin_perms.insert(static_cast<std::uint16_t>(P::BanUser));
    admin_perms.insert(static_cast<std::uint16_t>(P::UnbanUser));
    admin_perms.insert(static_cast<std::uint16_t>(P::BanIp));
    admin_perms.insert(static_cast<std::uint16_t>(P::UnbanIp));
    admin_perms.insert(static_cast<std::uint16_t>(P::SilenceUser));
    admin_perms.insert(static_cast<std::uint16_t>(P::UnsilenceUser));
    admin_perms.insert(static_cast<std::uint16_t>(P::CreateClan));
    admin_perms.insert(static_cast<std::uint16_t>(P::DisbandClan));
    admin_perms.insert(static_cast<std::uint16_t>(P::ViewAdminPanel));
    admin_perms.insert(static_cast<std::uint16_t>(P::ViewBanList));
    admin_perms.insert(static_cast<std::uint16_t>(P::ViewUserList));
    admin_perms.insert(static_cast<std::uint16_t>(P::ViewGameList));
    admin_perms.insert(static_cast<std::uint16_t>(P::ShutdownServer));
    admin_perms.insert(static_cast<std::uint16_t>(P::ReloadConfig));
    admin_perms.insert(static_cast<std::uint16_t>(P::ViewLogs));

    // Moderator group: user moderation only
    auto& mod_perms = group_permissions_["mod"];
    mod_perms.insert(static_cast<std::uint16_t>(P::KickFromChannel));
    mod_perms.insert(static_cast<std::uint16_t>(P::BanFromChannel));
    mod_perms.insert(static_cast<std::uint16_t>(P::KickUser));
    mod_perms.insert(static_cast<std::uint16_t>(P::SilenceUser));
    mod_perms.insert(static_cast<std::uint16_t>(P::ViewUserList));
    mod_perms.insert(static_cast<std::uint16_t>(P::ViewGameList));

    // Operator group: channel management
    auto& op_perms = group_permissions_["operator"];
    op_perms.insert(static_cast<std::uint16_t>(P::CreateChannel));
    op_perms.insert(static_cast<std::uint16_t>(P::SetChannelTopic));
    op_perms.insert(static_cast<std::uint16_t>(P::KickFromChannel));
    op_perms.insert(static_cast<std::uint16_t>(P::BanFromChannel));

    // Voice group: minimal permissions
    auto& voice_perms = group_permissions_["voice"];
    voice_perms.insert(static_cast<std::uint16_t>(P::SetChannelTopic));
}

bool InMemoryPermissionChecker::has_permission(
    domain::AccountId account, application::ports::Permission perm) const {
    // 1. Look up account
    auto account_result = accounts_->find_by_id(account);
    if (!account_result) {
        return false;
    }

    const auto& acc = account_result.value();

    // 2. Check each command group the account belongs to (groups 1-8)
    const auto& groups = acc.command_groups();
    for (std::uint8_t group_num = 1; group_num <= 8; ++group_num) {
        if (groups.has(group_num)) {
            std::string group_name;
            if (group_num == 1) group_name = "admin";
            else if (group_num == 2) group_name = "mod";
            else if (group_num == 3) group_name = "operator";
            else if (group_num == 4) group_name = "voice";
            
            auto it = group_permissions_.find(group_name);
            if (it != group_permissions_.end()) {
                auto perm_val = static_cast<std::uint16_t>(perm);
                if (it->second.count(perm_val) > 0) {
                    return true;
                }
            }
        }
    }

    return false;
}

bool InMemoryPermissionChecker::has_command_group(
    domain::AccountId account, std::string_view group) const {
    // 1. Look up account
    auto account_result = accounts_->find_by_id(account);
    if (!account_result) {
        return false;
    }

    const auto& acc = account_result.value();

    // 2. Check if group is in account's groups
    const auto& groups = acc.command_groups();
    for (std::uint8_t group_num = 1; group_num <= 8; ++group_num) {
        if (groups.has(group_num)) {
            std::string group_name;
            if (group_num == 1) group_name = "admin";
            else if (group_num == 2) group_name = "mod";
            else if (group_num == 3) group_name = "operator";
            else if (group_num == 4) group_name = "voice";
            
            if (group_name == group) {
                return true;
            }
        }
    }

    return false;
}

}  // namespace pvpgn::application::auth
