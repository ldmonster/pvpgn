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
#include <string_view>
#include <vector>

#include "application/ports/account_repository.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/identity/account.hpp"

namespace pvpgn::integration::legacy_bnetd {

// R194: signatures track the current `IAccountRepository` interface
// (R166+). All read-path methods (`find_by_id`, `find_by_name`,
// `save`, `exists`) are implemented against the legacy `t_account`
// list; `remove`, `list_online`, and `count` are intentional stubs
// surfaced as `StatusCode::Internal` for now (no caller uses them
// in the WITH_BNETD=ON path).
class LegacyAccountRepository final
    : public application::ports::IAccountRepository {
public:
    core::Result<domain::identity::Account, core::Error>
    find_by_id(std::uint32_t id) override;

    core::Result<domain::identity::Account, core::Error>
    find_by_name(std::string_view name) override;

    core::Result<void, core::Error>
    save(const domain::identity::Account& account) override;

    core::Result<void, core::Error>
    remove(std::string_view name) override;

    core::Result<bool, core::Error>
    exists(std::string_view name) override;

    core::Result<std::vector<domain::identity::Account>, core::Error>
    list_online() override;

    core::Result<std::uint32_t, core::Error>
    count() override;
};

}  // namespace pvpgn::integration::legacy_bnetd
