// SPDX-License-Identifier: GPL-2.0-or-later

#include "application/icon_table/icon_table.hpp"

#include <cstdio>
#include <cstring>

namespace pvpgn::application::icon_table {

namespace {

std::array<char, 4> default_user_icon(const AccountIconContext& ctx) {
    // Legacy: snprintf("%1d%c3W", rlvl, rico).
    std::array<char, 4> out{};
    char buf[8]{};
    std::snprintf(buf, sizeof(buf), "%1d%c3W",
                  static_cast<int>(ctx.race_icon_level), ctx.race_icon_char);
    out[0] = buf[0];
    out[1] = buf[1];
    out[2] = buf[2];
    out[3] = buf[3];
    return out;
}

std::uint16_t threshold_for(
    const IconReqTable& req,
    Clienttag tag,
    int row_index,            // 0-based row j
    bool is_tourney_column) {
    if (is_tourney_column) {
        if (row_index < 0 ||
            row_index >= static_cast<int>(req.tourney.size())) return 0;
        return req.tourney[static_cast<std::size_t>(row_index)];
    }
    if (tag == Clienttag::War3) {
        if (row_index < 0 ||
            row_index >= static_cast<int>(req.war3.size())) return 0;
        return req.war3[static_cast<std::size_t>(row_index)];
    }
    // W3xp
    if (row_index < 0 ||
        row_index >= static_cast<int>(req.w3xp.size())) return 0;
    return req.w3xp[static_cast<std::size_t>(row_index)];
}

}  // namespace

IconReplyTable build_icon_reply_table(
    Clienttag                  tag,
    const IconReqTable&        req,
    const AccountIconContext&  ctx,
    PortraitResolver           resolver,
    void*                      resolver_user) {

    IconReplyTable out{};
    if (tag == Clienttag::War3) { out.width = 5; out.height = 4; }
    else                        { out.width = 6; out.height = 5; }

    // curricon: user_icon overrides race-icon-derived default;
    // custom_icon overrides both (only when no explicit user_icon).
    if (ctx.user_icon) {
        out.curricon = *ctx.user_icon;
    } else {
        out.curricon = default_user_icon(ctx);
        if (ctx.custom_icon) out.curricon = *ctx.custom_icon;
    }

    const bool assigned_custom = !ctx.user_icon && ctx.custom_icon.has_value();

    out.entries.reserve(static_cast<std::size_t>(out.width) * out.height);
    constexpr std::array<char, 5> icon_pos{'2', '3', '4', '5', '6'};

    for (int j = 0; j < out.height; ++j) {
        for (int i = 0; i < out.width; ++i) {
            IconEntry e{};
            e.icon_code = {icon_pos[static_cast<std::size_t>(j)],
                           kRaceChars[static_cast<std::size_t>(i)],
                           '3', 'W'};
            e.race      = static_cast<std::uint8_t>(i);
            e.portrait_code = resolver
                ? resolver(e.icon_code, resolver_user)
                : 0u;

            const bool is_tourney_col = (i == 5);  // only present on W3xp
            const std::uint16_t threshold =
                threshold_for(req, tag, j, is_tourney_col);
            e.required_wins = threshold;

            const auto wins =
                ctx.race_wins[static_cast<std::size_t>(i)];
            const bool unlocked = !assigned_custom && wins >= threshold;
            e.client_enabled = unlocked ? 1u : 0u;

            out.entries.push_back(e);
        }
    }
    return out;
}

bool validate_user_icon(
    const IconReqTable&                req,
    std::array<char, 4>                icon,
    const std::array<std::uint32_t, 6>& race_wins) {

    // Default icon "1O3W" is always allowed: the legacy code calls
    // this the fallback when the player has nothing unlocked.
    if (icon[0] == '1' && icon[1] == 'O' && icon[2] == '3' && icon[3] == 'W') {
        return true;
    }

    const char level_char = icon[0];
    const char race_char  = icon[1];

    // Level digit must be '2'..'6'; map to row index 0..4.
    if (level_char < '2' || level_char > '6') return false;
    const int row = level_char - '2';

    // Find the race column.
    int col = -1;
    for (int i = 0; i < static_cast<int>(kRaceChars.size()); ++i) {
        if (kRaceChars[static_cast<std::size_t>(i)] == race_char) { col = i; break; }
    }
    if (col < 0) return false;

    // 'D' (col == 5) is the tournament column. Every other race
    // uses the W3XP race-win thresholds (the legacy validator
    // hard-codes the W3XP series 25/150/350/750/1500 regardless of
    // clienttag, matching the bnetd_default behaviour).
    const std::uint16_t threshold =
        (col == 5)
            ? (row < static_cast<int>(req.tourney.size())
                   ? req.tourney[static_cast<std::size_t>(row)] : 0)
            : (row < static_cast<int>(req.w3xp.size())
                   ? req.w3xp[static_cast<std::size_t>(row)]    : 0);

    return race_wins[static_cast<std::size_t>(col)] >= threshold;
}

}  // namespace pvpgn::application::icon_table
