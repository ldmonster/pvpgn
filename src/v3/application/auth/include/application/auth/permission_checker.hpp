// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file permission_checker.hpp
/// In-memory implementation of IPermissionChecker.
///
/// Backed by IAccountRepository; looks up command_groups from
/// Account aggregate and maps known group names to Permission sets.

#include <memory>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "application/ports/permission_checker.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::ports {
class IAccountRepository;
}

namespace pvpgn::application::auth {

class InMemoryPermissionChecker : public application::ports::IPermissionChecker {
public:
    explicit InMemoryPermissionChecker(
        std::shared_ptr<application::ports::IAccountRepository> accounts)
        : accounts_(accounts) {
        init_group_mappings();
    }

    bool has_permission(domain::AccountId account,
                        application::ports::Permission perm) const override;

    bool has_command_group(domain::AccountId account,
                           std::string_view group) const override;

private:
    std::shared_ptr<application::ports::IAccountRepository> accounts_;

    // Maps command groups to their associated Permission sets
    std::unordered_map<std::string, std::unordered_set<std::uint16_t>>
        group_permissions_;

    void init_group_mappings();
};

}  // namespace pvpgn::application::auth
