// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/shadow/shadow_unit_of_work.hpp"

namespace pvpgn::infra::shadow {

// ============================================================================
// ShadowUnitOfWork (reference-owning variant)
// ============================================================================

ShadowUnitOfWork::ShadowUnitOfWork(
    application::ports::IUnitOfWork& primary,
    application::ports::IUnitOfWork& secondary,
    bool enabled)
    : primary_(primary),
      secondary_(secondary),
      enabled_(enabled),
      shadow_accounts_(std::make_unique<ShadowAccountRepository>(
          primary_.accounts(), secondary_.accounts(), enabled_)) {}

core::Result<void, core::Error> ShadowUnitOfWork::begin() {
    auto result = primary_.begin();
    if (!result.has_value()) {
        return result;
    }
    if (enabled_) {
        (void)secondary_.begin();
    }
    return result;
}

core::Result<void, core::Error> ShadowUnitOfWork::commit() {
    auto result = primary_.commit();
    if (!result.has_value()) {
        return result;
    }
    if (enabled_) {
        (void)secondary_.commit();
    }
    return result;
}

void ShadowUnitOfWork::rollback() noexcept {
    primary_.rollback();
    if (enabled_) {
        secondary_.rollback();
    }
}

application::ports::IAccountRepository& ShadowUnitOfWork::accounts() {
    return *shadow_accounts_;
}

application::ports::IChannelRepository& ShadowUnitOfWork::channels() {
    return primary_.channels();
}

application::ports::IGameRepository& ShadowUnitOfWork::games() {
    return primary_.games();
}

application::ports::IClanRepository& ShadowUnitOfWork::clans() {
    return primary_.clans();
}

application::ports::ILadderRepository& ShadowUnitOfWork::ladder() {
    return primary_.ladder();
}

application::ports::IIpBanRepository& ShadowUnitOfWork::ip_bans() {
    return primary_.ip_bans();
}

application::ports::IAccountBanRepository& ShadowUnitOfWork::account_bans() {
    return primary_.account_bans();
}

application::ports::IFriendListRepository& ShadowUnitOfWork::friend_lists() {
    return primary_.friend_lists();
}

application::ports::IRealmRepository& ShadowUnitOfWork::realms() {
    return primary_.realms();
}

application::ports::ITeamRepository& ShadowUnitOfWork::teams() {
    return primary_.teams();
}

// ============================================================================
// OwningShadowUnitOfWork (owning variant — used by ShadowUnitOfWorkFactory)
// ============================================================================

OwningShadowUnitOfWork::OwningShadowUnitOfWork(
    std::unique_ptr<application::ports::IUnitOfWork> primary,
    std::unique_ptr<application::ports::IUnitOfWork> secondary,
    bool enabled)
    : primary_owned_(std::move(primary)),
      secondary_owned_(std::move(secondary)),
      delegate_(std::make_unique<ShadowUnitOfWork>(
          *primary_owned_, *secondary_owned_, enabled)) {}

core::Result<void, core::Error> OwningShadowUnitOfWork::begin() {
    return delegate_->begin();
}

core::Result<void, core::Error> OwningShadowUnitOfWork::commit() {
    return delegate_->commit();
}

void OwningShadowUnitOfWork::rollback() noexcept {
    delegate_->rollback();
}

application::ports::IAccountRepository& OwningShadowUnitOfWork::accounts() {
    return delegate_->accounts();
}

application::ports::IChannelRepository& OwningShadowUnitOfWork::channels() {
    return delegate_->channels();
}

application::ports::IGameRepository& OwningShadowUnitOfWork::games() {
    return delegate_->games();
}

application::ports::IClanRepository& OwningShadowUnitOfWork::clans() {
    return delegate_->clans();
}

application::ports::ILadderRepository& OwningShadowUnitOfWork::ladder() {
    return delegate_->ladder();
}

application::ports::IIpBanRepository& OwningShadowUnitOfWork::ip_bans() {
    return delegate_->ip_bans();
}

application::ports::IAccountBanRepository& OwningShadowUnitOfWork::account_bans() {
    return delegate_->account_bans();
}

application::ports::IFriendListRepository& OwningShadowUnitOfWork::friend_lists() {
    return delegate_->friend_lists();
}

application::ports::IRealmRepository& OwningShadowUnitOfWork::realms() {
    return delegate_->realms();
}

application::ports::ITeamRepository& OwningShadowUnitOfWork::teams() {
    return delegate_->teams();
}

}  // namespace pvpgn::infra::shadow
