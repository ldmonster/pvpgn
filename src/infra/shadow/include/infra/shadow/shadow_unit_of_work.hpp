// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file shadow_unit_of_work.hpp
/// Shadow-write Unit of Work adapters.
///
/// Two variants are provided:
///   - ShadowUnitOfWork: wraps two IUnitOfWork references (caller owns them).
///   - OwningShadowUnitOfWork: owns both IUnitOfWork instances via unique_ptr
///     (used by ShadowUnitOfWorkFactory which creates both UoWs per call).

#include <memory>

#include "application/persistence/unit_of_work.hpp"
#include "infra/shadow/shadow_account_repository.hpp"

namespace pvpgn::infra::shadow {

/// Shadow-write Unit of Work (reference-owning variant).
///
/// Wraps a primary and secondary IUnitOfWork by reference. begin/commit/rollback
/// are forwarded to both. The accounts() accessor returns a ShadowAccountRepository
/// that mirrors writes to both backends. All other repository accessors delegate
/// to the primary only.
///
/// Lifetime: both @p primary and @p secondary must outlive this object.
class ShadowUnitOfWork final : public application::ports::IUnitOfWork {
public:
    /// @param primary   The authoritative Unit of Work.
    /// @param secondary The mirror Unit of Work.
    /// @param enabled   Feature flag — when false, behaves as a pass-through to primary.
    ShadowUnitOfWork(application::ports::IUnitOfWork& primary,
                     application::ports::IUnitOfWork& secondary,
                     bool enabled);

    // ---- Transaction lifecycle -------------------------------------------

    core::Result<void, core::Error> begin() override;
    core::Result<void, core::Error> commit() override;
    void rollback() noexcept override;

    // ---- Repository accessors --------------------------------------------

    /// Returns a ShadowAccountRepository that mirrors writes to both backends.
    application::ports::IAccountRepository& accounts() override;

    /// All other repositories delegate to primary only.
    application::ports::IChannelRepository&     channels() override;
    application::ports::IGameRepository&        games() override;
    application::ports::IClanRepository&        clans() override;
    application::ports::ILadderRepository&      ladder() override;
    application::ports::IIpBanRepository&       ip_bans() override;
    application::ports::IAccountBanRepository&  account_bans() override;
    application::ports::IFriendListRepository&  friend_lists() override;
    application::ports::IRealmRepository&       realms() override;
    [[nodiscard]] application::ports::ITeamRepository& teams() override;

private:
    application::ports::IUnitOfWork& primary_;
    application::ports::IUnitOfWork& secondary_;
    bool enabled_;

    /// Lazily constructed shadow account repository (wraps primary + secondary accounts).
    std::unique_ptr<ShadowAccountRepository> shadow_accounts_;
};

/// Shadow-write Unit of Work (owning variant).
///
/// Owns both IUnitOfWork instances via unique_ptr. Used by
/// ShadowUnitOfWorkFactory which creates a new pair of UoWs per call.
class OwningShadowUnitOfWork final : public application::ports::IUnitOfWork {
public:
    OwningShadowUnitOfWork(std::unique_ptr<application::ports::IUnitOfWork> primary,
                           std::unique_ptr<application::ports::IUnitOfWork> secondary,
                           bool enabled);

    // ---- Transaction lifecycle -------------------------------------------

    core::Result<void, core::Error> begin() override;
    core::Result<void, core::Error> commit() override;
    void rollback() noexcept override;

    // ---- Repository accessors --------------------------------------------

    application::ports::IAccountRepository& accounts() override;
    application::ports::IChannelRepository&     channels() override;
    application::ports::IGameRepository&        games() override;
    application::ports::IClanRepository&        clans() override;
    application::ports::ILadderRepository&      ladder() override;
    application::ports::IIpBanRepository&       ip_bans() override;
    application::ports::IAccountBanRepository&  account_bans() override;
    application::ports::IFriendListRepository&  friend_lists() override;
    application::ports::IRealmRepository&       realms() override;
    [[nodiscard]] application::ports::ITeamRepository& teams() override;

private:
    std::unique_ptr<application::ports::IUnitOfWork> primary_owned_;
    std::unique_ptr<application::ports::IUnitOfWork> secondary_owned_;

    /// Delegates to the reference-based ShadowUnitOfWork.
    std::unique_ptr<ShadowUnitOfWork> delegate_;
};

}  // namespace pvpgn::infra::shadow
