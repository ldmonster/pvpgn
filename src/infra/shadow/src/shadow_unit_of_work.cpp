// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/shadow/shadow_unit_of_work.hpp"
#include "application/persistence/unit_of_work.hpp"
#include "domain/chat/ports.hpp"
#include "domain/gameplay/ports.hpp"
#include "domain/identity/ports.hpp"
#include "domain/ladder/ports.hpp"
#include "domain/moderation/ports.hpp"
#include "domain/realm/ports.hpp"
#include "domain/social/ports.hpp"

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

domain::identity::IAccountRepository& ShadowUnitOfWork::accounts() {
    return *shadow_accounts_;
}

domain::chat::IChannelRepository& ShadowUnitOfWork::channels() {
    return primary_.channels();
}

domain::gameplay::IGameRepository& ShadowUnitOfWork::games() {
    return primary_.games();
}

domain::social::IClanRepository& ShadowUnitOfWork::clans() {
    return primary_.clans();
}

domain::ladder::ILadderRepository& ShadowUnitOfWork::ladder() {
    return primary_.ladder();
}

domain::moderation::IIpBanRepository& ShadowUnitOfWork::ip_bans() {
    return primary_.ip_bans();
}

domain::moderation::IAccountBanRepository& ShadowUnitOfWork::account_bans() {
    return primary_.account_bans();
}

domain::social::IFriendListRepository& ShadowUnitOfWork::friend_lists() {
    return primary_.friend_lists();
}

domain::realm::IRealmRepository& ShadowUnitOfWork::realms() {
    return primary_.realms();
}

domain::social::ITeamRepository& ShadowUnitOfWork::teams() {
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

domain::identity::IAccountRepository& OwningShadowUnitOfWork::accounts() {
    return delegate_->accounts();
}

domain::chat::IChannelRepository& OwningShadowUnitOfWork::channels() {
    return delegate_->channels();
}

domain::gameplay::IGameRepository& OwningShadowUnitOfWork::games() {
    return delegate_->games();
}

domain::social::IClanRepository& OwningShadowUnitOfWork::clans() {
    return delegate_->clans();
}

domain::ladder::ILadderRepository& OwningShadowUnitOfWork::ladder() {
    return delegate_->ladder();
}

domain::moderation::IIpBanRepository& OwningShadowUnitOfWork::ip_bans() {
    return delegate_->ip_bans();
}

domain::moderation::IAccountBanRepository& OwningShadowUnitOfWork::account_bans() {
    return delegate_->account_bans();
}

domain::social::IFriendListRepository& OwningShadowUnitOfWork::friend_lists() {
    return delegate_->friend_lists();
}

domain::realm::IRealmRepository& OwningShadowUnitOfWork::realms() {
    return delegate_->realms();
}

domain::social::ITeamRepository& OwningShadowUnitOfWork::teams() {
    return delegate_->teams();
}

}  // namespace pvpgn::infra::shadow
