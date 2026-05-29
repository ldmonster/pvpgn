// SPDX-License-Identifier: GPL-2.0-or-later
//
// Adapter that snapshots a legacy `t_account` into the pure
// `application::icon_table::AccountIconContext` consumed by
// `build_icon_reply_table`. Lives in the linked integration target
// because it must call into `bnetd_legacy` (account_wrap, prefs,
// icons.h).
//
// All legacy types are erased into `void*` at the v3 boundary so
// callers in the v3 layer never include legacy headers.

#pragma once

#include <array>
#include <cstdint>

#include "application/icon_table/icon_table.hpp"

namespace pvpgn::integration::legacy_bnetd {

/// Build an `AccountIconContext` from the legacy account state. Pass
/// `account` as a `t_account*` cast to `void*`. Returns a context
/// suitable for `application::icon_table::build_icon_reply_table`.
application::icon_table::AccountIconContext
build_icon_account_context(void* account, std::uint32_t clienttag);

/// Per-call user-data for `portrait_resolver_fn`.
struct PortraitResolverCtx {
    void*         account   = nullptr;  // t_account*
    std::uint32_t clienttag = 0;
};

/// `PortraitResolver` callback. The `user` argument must point to a
/// `PortraitResolverCtx`. Calls `account_icon_to_profile_icon`
/// internally.
std::uint32_t portrait_resolver_fn(
    std::array<char, 4> icon_code, void* user);

}  // namespace pvpgn::integration::legacy_bnetd
