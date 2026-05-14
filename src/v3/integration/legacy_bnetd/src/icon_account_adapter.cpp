// SPDX-License-Identifier: GPL-2.0-or-later
//
// Implementation of icon_account_adapter -- pulls AccountIconContext
// state out of legacy bnetd globals/getters. Lives in the linked
// integration target.

#include "integration/legacy_bnetd/icon_account_adapter.hpp"

#include <cstring>

#include "common/setup_before.h"
#include "common/eventlog.h"
#include "common/tag.h"
#include "bnetd/account.h"
#include "bnetd/account_wrap.h"
#include "bnetd/icons.h"
#include "common/setup_after.h"

namespace pvpgn::integration::legacy_bnetd {

namespace {

// Race index -> legacy `t_w3race` integer encoding used by
// account_get_racewins (matches kRaceCodes ordering in icon_table).
inline constexpr std::array<unsigned int, 6> kRaceUints{
    0u /*RANDOM*/, 1u /*HUMANS*/, 2u /*ORCS*/, 3u /*UNDEAD*/,
    4u /*NIGHTELVES*/, 5u /*DEMONS*/};

}  // namespace

application::icon_table::AccountIconContext
build_icon_account_context(void* account_v, std::uint32_t clienttag_u) {
    using application::icon_table::AccountIconContext;
    AccountIconContext ctx{};

    auto* account   = static_cast<pvpgn::bnetd::t_account*>(account_v);
    auto  clienttag = static_cast<pvpgn::t_clienttag>(clienttag_u);
    if (!account) return ctx;

    // user_icon (4 raw bytes) -- NULL means no explicit selection.
    if (char const* uicon =
            pvpgn::bnetd::account_get_user_icon(account, clienttag)) {
        std::array<char, 4> u{};
        std::memcpy(u.data(), uicon, 4);
        ctx.user_icon = u;
    } else {
        // race-icon fallback (rico = race char, rlvl = level digit).
        char         rico = 'R';
        unsigned int rlvl = 1;
        unsigned int rwins = 0;
        pvpgn::bnetd::account_get_raceicon(
            account, &rico, &rlvl, &rwins, clienttag);
        ctx.race_icon_char  = rico;
        ctx.race_icon_level = static_cast<std::uint8_t>(rlvl);
    }

    // Per-race wins. Even when an explicit user_icon is selected the
    // legacy code still computes table cells (race-win checks gate
    // the Set-Icon picker on the client side).
    for (std::size_t i = 0; i < kRaceUints.size(); ++i) {
        const int wins = pvpgn::bnetd::account_get_racewins(
            account, kRaceUints[i], clienttag);
        ctx.race_wins[i] =
            (wins < 0) ? 0u : static_cast<std::uint32_t>(wins);
    }

    // custom_icon (only when no explicit user_icon).
    if (!ctx.user_icon
        && pvpgn::bnetd::prefs_get_custom_icons() == 1
        && pvpgn::bnetd::customicons_allowed_by_client(clienttag)) {
        if (auto* icon =
                pvpgn::bnetd::customicons_get_icon_by_account(account, clienttag);
            icon && icon->icon_code) {
            std::array<char, 4> c{};
            std::memcpy(c.data(), icon->icon_code, 4);
            ctx.custom_icon = c;
        }
    }

    return ctx;
}

std::uint32_t portrait_resolver_fn(
    std::array<char, 4> icon_code, void* user) {
    auto* ctx = static_cast<PortraitResolverCtx*>(user);
    if (!ctx || !ctx->account) return 0u;
    auto* account   = static_cast<pvpgn::bnetd::t_account*>(ctx->account);
    auto  clienttag = static_cast<pvpgn::t_clienttag>(ctx->clienttag);
    return pvpgn::bnetd::account_icon_to_profile_icon(
        icon_code.data(), account, clienttag);
}

}  // namespace pvpgn::integration::legacy_bnetd
