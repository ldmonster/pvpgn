// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file issue_warning.hpp
/// ISSUE_WARNING use-case — record a formal warning against an account.

#include <cstdint>
#include <memory>
#include <string>

#include "domain/identity/ports.hpp"
#include "domain/moderation/ports.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::moderation {

struct IssueWarningCommand {
    domain::AccountId  admin_id;
    std::string        admin_name;
    std::string        target_account_name;
    std::string        reason;
    std::uint32_t      warning_level;  ///< 1–3
};

enum class IssueWarningError : std::uint8_t {
    TargetNotFound,
    InvalidWarningLevel,
    InvalidReason,
};

class IssueWarning {
public:
    IssueWarning(std::shared_ptr<domain::identity::IAccountRepository> accounts,
                 std::shared_ptr<domain::moderation::IAuditLog> audit)
        : accounts_(accounts), audit_(audit) {}

    /// Returns TargetNotFound if the target account does not exist.
    /// Returns InvalidWarningLevel if warning_level is not in [1, 3].
    /// Returns InvalidReason if reason is empty.
    [[nodiscard]] core::Result<void, IssueWarningError>
    execute(const IssueWarningCommand& cmd) const;

private:
    std::shared_ptr<domain::identity::IAccountRepository> accounts_;
    std::shared_ptr<domain::moderation::IAuditLog>          audit_;
};

}  // namespace pvpgn::application::moderation
