// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file shadow_account_repository.hpp
/// Shadow-write adapter for IAccountRepository.
///
/// Wraps a primary and secondary IAccountRepository. All reads are served
/// from the primary. All writes are applied to the primary first; if the
/// shadow is enabled, the same write is then mirrored to the secondary.
/// This enables zero-downtime migration: run both backends in parallel,
/// verify consistency, then cut over.

#include <cstddef>
#include <functional>

#include "domain/identity/ports.hpp"

namespace pvpgn::infra::shadow {

/// Shadow-write adapter for IAccountRepository.
///
/// When @p enabled is true, every mutating operation (save, remove) is
/// applied to both @p primary and @p secondary. Read operations always
/// delegate to @p primary only.
///
/// Lifetime: both @p primary and @p secondary must outlive this object.
class ShadowAccountRepository final
    : public application::ports::IAccountRepository {
public:
    /// @param primary   The authoritative backend (reads + writes always go here).
    /// @param secondary The mirror backend (writes go here when enabled).
    /// @param enabled   Feature flag — when false, behaves as a pass-through to primary.
    ShadowAccountRepository(application::ports::IAccountRepository& primary,
                            application::ports::IAccountRepository& secondary,
                            bool enabled);

    // ---- Read operations — always delegate to primary --------------------

    core::Result<domain::identity::Account>
    find_by_id(domain::AccountId id) const override;

    core::Result<domain::identity::Account>
    find_by_name(const domain::UserName& name) const override;

    void forEach(
        std::function<bool(const domain::identity::Account&)> predicate)
        const override;

    std::size_t size() const noexcept override;

    // ---- Write operations — primary first, then secondary (if enabled) ---

    core::Status<>
    save(const domain::identity::Account& account) override;

    core::Status<>
    remove(domain::AccountId id) override;

private:
    application::ports::IAccountRepository& primary_;
    application::ports::IAccountRepository& secondary_;
    bool enabled_;
};

}  // namespace pvpgn::infra::shadow
