// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/moderation/silence_user.hpp"

#include "domain/identity/ports.hpp"
#include "domain/shared/event_bus.hpp"

namespace pvpgn::application::moderation {

core::Result<void, SilenceUserError>
SilenceUser::execute(const SilenceUserRequest& req) {
    // 1. Verify target account exists
    auto target_result = accounts_->find_by_id(req.target);
    if (!target_result) {
        return core::fail(SilenceUserError::TargetNotFound);
    }

    // 2. Validate duration
    if (req.duration.count() <= 0) {
        return core::fail(SilenceUserError::InvalidDuration);
    }

    // 3. Validate reason
    if (req.reason.empty()) {
        return core::fail(SilenceUserError::InvalidReason);
    }

    // 4. Update account with silence end time
    // This would require Account aggregate support for silence tracking
    // For now, would need to be implemented in Account::silence()
    auto account = target_result.value();
    // auto silence_result = account.silence(req.duration);

    // 5. Save updated account
    auto save_result = accounts_->save(account);
    if (!save_result) {
        return core::fail(SilenceUserError::PersistenceFailed);
    }

    // 6. Publish events (would include UserSilenced domain event)
    // Events would be published via event_bus_

    return core::Result<void, SilenceUserError>{};
}

}  // namespace pvpgn::application::moderation
