// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file legacy_account_repository.hpp
/// Read-path `IAccountRepository` adapter backed by the legacy
/// `t_account` list (Batch 28a). The adapter is intentionally
/// minimal:
///
///   * `find_by_id` / `find_by_name` resolve a legacy `t_account`,
///     hex-decode its `passhash1` attribute into a `BNHash`, and
///     rehydrate an `identity::Account` aggregate.
///   * `save()`, `remove()`, `size()` are *stubs* in this batch:
///     `save()` returns OK without touching the legacy account (the
///     ChangePassword wire-in in 28b expects to call `save()` after
///     `change_password()` and then sync the legacy attributes
///     separately via the bridge); `remove()` is unimplemented.
///
/// All TU-level dependencies on the legacy headers live in the
/// linked variant of `integration_legacy_bnetd`; this header keeps
/// its public surface free of `t_account` so the unlinked variant
/// (and tests) can still compile.

#include <cstddef>
#include <cstdint>
#include <functional>

#include "application/ports/account_repository.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/identity/account.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/user_name.hpp"

namespace pvpgn::integration::legacy_bnetd {

// R194: signatures track the current `IAccountRepository` interface.
// All read-path methods (`find_by_id`, `find_by_name`, `save`) are
// implemented against the legacy `t_account` list; `remove`, `forEach`,
// and `size` are intentional stubs surfaced as `StatusCode::Internal`
// for now (no caller uses them in the WITH_BNETD=ON path).
class LegacyAccountRepository final
    : public application::ports::IAccountRepository {
public:
    core::Result<domain::identity::Account>
    find_by_id(domain::AccountId id) const override;

    core::Result<domain::identity::Account>
    find_by_name(const domain::UserName& name) const override;

    core::Status<>
    save(const domain::identity::Account& account) override;

    core::Status<>
    remove(domain::AccountId id) override;

    void forEach(
        std::function<bool(const domain::identity::Account&)> predicate)
        const override;

    std::size_t size() const noexcept override;
};

}  // namespace pvpgn::integration::legacy_bnetd
