// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/shadow/shadow_account_repository.hpp"

namespace pvpgn::infra::shadow {

ShadowAccountRepository::ShadowAccountRepository(
    application::ports::IAccountRepository& primary,
    application::ports::IAccountRepository& secondary,
    bool enabled)
    : primary_(primary), secondary_(secondary), enabled_(enabled) {}

// ---- Read operations — always delegate to primary ------------------------

core::Result<domain::identity::Account>
ShadowAccountRepository::find_by_id(domain::AccountId id) const {
    return primary_.find_by_id(id);
}

core::Result<domain::identity::Account>
ShadowAccountRepository::find_by_name(const domain::UserName& name) const {
    return primary_.find_by_name(name);
}

void ShadowAccountRepository::forEach(
    std::function<bool(const domain::identity::Account&)> predicate) const {
    primary_.forEach(std::move(predicate));
}

std::size_t ShadowAccountRepository::size() const noexcept {
    return primary_.size();
}

// ---- Write operations — primary first, then secondary (if enabled) -------

core::Status<>
ShadowAccountRepository::save(const domain::identity::Account& account) {
    // Always write to primary first
    auto result = primary_.save(account);
    if (!result.has_value()) {
        return result;
    }

    // Mirror to secondary only when shadow is enabled
    if (enabled_) {
        // Best-effort: ignore secondary errors to avoid blocking primary
        (void)secondary_.save(account);
    }

    return result;
}

core::Status<>
ShadowAccountRepository::remove(domain::AccountId id) {
    // Always remove from primary first
    auto result = primary_.remove(id);
    if (!result.has_value()) {
        return result;
    }

    // Mirror to secondary only when shadow is enabled
    if (enabled_) {
        // Best-effort: ignore secondary errors to avoid blocking primary
        (void)secondary_.remove(id);
    }

    return result;
}

}  // namespace pvpgn::infra::shadow
