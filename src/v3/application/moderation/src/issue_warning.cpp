// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/moderation/issue_warning.hpp"

#include "application/ports/account_repository.hpp"
#include "application/ports/audit_log.hpp"
#include "core/clock.hpp"

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
    auto account_result = accounts_->find_by_name(cmd.target_account_name);
    if (!account_result) {
        return core::fail(IssueWarningError::TargetNotFound);
    }

    // 4. Record audit entry
    audit_->record(application::ports::AuditEntry{
        .action    = application::ports::AuditAction::AccountLocked,
        .actor     = cmd.admin_id,
        .subject   = cmd.target_account_name,
        .details   = cmd.reason,
        .source_ip = {},
        .timestamp = std::chrono::system_clock::now(),
    });

    return core::Result<void, IssueWarningError>{};
}

}  // namespace pvpgn::application::moderation
