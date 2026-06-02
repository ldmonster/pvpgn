// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/moderation/issue_warning.hpp"

#include "domain/identity/ports.hpp"
#include "domain/moderation/ports.hpp"
#include "core/clock.hpp"
#include "domain/shared/user_name.hpp"

namespace pvpgn::application::moderation {

core::Result<void, IssueWarningError>
IssueWarning::execute(const IssueWarningCommand& cmd) const {
    // 1. Validate warning level
    if (cmd.warning_level < 1 || cmd.warning_level > 3) {
        return core::fail(IssueWarningError::InvalidWarningLevel);
    }

    // 2. Validate reason
    if (cmd.reason.empty()) {
        return core::fail(IssueWarningError::InvalidReason);
    }

    // 3. Look up target account by name
    auto parsed_name = domain::UserName::parse(cmd.target_account_name);
    if (!parsed_name) {
        return core::fail(IssueWarningError::TargetNotFound);
    }
    auto account_result = accounts_->find_by_name(parsed_name.value());
    if (!account_result) {
        return core::fail(IssueWarningError::TargetNotFound);
    }

    // 4. Record audit entry
    audit_->record(domain::moderation::AuditEntry{
        .action    = domain::moderation::AuditAction::AccountLocked,
        .actor     = cmd.admin_id,
        .subject   = cmd.target_account_name,
        .details   = cmd.reason,
        .source_ip = {},
        .timestamp = std::chrono::system_clock::now(),
    });

    return core::Result<void, IssueWarningError>{};
}

}  // namespace pvpgn::application::moderation
