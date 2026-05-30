--[[
    /stats command — display DotA statistics for a player.
    Only active for Warcraft III Expansion clients when DotA server mode is enabled.

    Migrated from scripts/lua/command/stats.lua + scripts/lua/ghost/command.lua (gh_command_stats)
    Copyright (C) 2014 HarpyWar (harpywar@gmail.com)
    This file is a part of the PvPGN Project http://pvpgn.pro
    Licensed under the same terms as Lua itself.
]]--

local helper = require("helper")

-- Warcraft III Expansion client tag
local CLIENTTAG_WAR3XP = "W3XP"

local M = {}

-- Retrieve all DotA stats fields for an account
local function get_dotastats(account_name)
    local stats = {
        rating5x5         = pvpgn.db.get_dotarating_5x5(account_name)  or 0,
        rating3x3         = pvpgn.db.get_dotarating_3x3(account_name)  or 0,
        wins5x5           = pvpgn.db.get_dotawins_5x5(account_name)    or 0,
        wins3x3           = pvpgn.db.get_dotawins_3x3(account_name)    or 0,
        losses5x5         = pvpgn.db.get_dotalosses_5x5(account_name)  or 0,
        losses3x3         = pvpgn.db.get_dotalosses_3x3(account_name)  or 0,
        streaks5x5        = pvpgn.db.get_dotastreaks_5x5(account_name) or 0,
        streaks3x3        = pvpgn.db.get_dotastreaks_3x3(account_name) or 0,
        leaves5x5         = pvpgn.db.get_dotaleaves_5x5(account_name)  or 0,
        leaves3x3         = pvpgn.db.get_dotaleaves_3x3(account_name)  or 0,
    }

    -- Derive rank strings (placeholder — real ranking logic lives in the DB layer)
    stats.rank5x5 = pvpgn.db.get_dotarank_5x5 and pvpgn.db.get_dotarank_5x5(account_name) or "-"
    stats.rank3x3 = pvpgn.db.get_dotarank_3x3 and pvpgn.db.get_dotarank_3x3(account_name) or "-"

    -- Country (first 2 chars)
    local acc = pvpgn.account.find_by_name(account_name)
    stats.country = (acc and acc.country and acc.country:sub(1, 2)) or "-"

    -- Leave percentages (guard against division by zero)
    local total5 = stats.wins5x5 + stats.losses5x5
    local total3 = stats.wins3x3 + stats.losses3x3
    stats.leaves5x5_percent = total5 > 0 and math.floor(stats.leaves5x5 / (total5 / 100) * 10) / 10 or 0.0
    stats.leaves3x3_percent = total3 > 0 and math.floor(stats.leaves3x3 / (total3 / 100) * 10) / 10 or 0.0
    -- Combined leave count/percent
    stats.leaves         = stats.leaves5x5 + stats.leaves3x3
    local total_all      = total5 + total3
    stats.leaves_percent = total_all > 0 and
        string.format("%.1f", (stats.leaves / (total_all / 100))) or "0.0"

    return stats
end

-- Handle /stats [username] command
function M.handle(session, args)
    -- Only for W3XP clients when dota_server is enabled
    if session.clienttag and session.clienttag ~= CLIENTTAG_WAR3XP then
        return 1
    end
    if pvpgn.config.get("dota_server") == "false" then
        return 1
    end

    local target_name = args and args[1]
    local target_acc

    if target_name then
        target_acc = pvpgn.account.find_by_name(target_name)
        if not target_acc then
            pvpgn.chat.send_whisper(session.account_name, nil,
                pvpgn.i18n.localize(session.account_name, "Invalid user."))
            return -1
        end
    else
        target_acc = pvpgn.account.find_by_name(session.account_name)
    end

    local stats = get_dotastats(target_acc.name)
    local pts   = pvpgn.i18n.localize(session.account_name, "pts")

    -- If caller is in a GHost-hosted ladder game and no explicit target, show all players
    local game = pvpgn.game.find_by_id(session.game_id)
    local owner = game and helper.find_userbot_by_game(game.name)
    local gametype = owner and helper.get_userbot_gametype(owner) or ""

    if not target_name and game and next(game) and gametype ~= "" then
        -- Show stats for every player in the game
        for u in (game.players or ""):gmatch("[^,]+") do
            u = u:match("^%s*(.-)%s*$")
            local s = get_dotastats(u)
            local rank    = gametype == "3x3" and s.rank3x3    or s.rank5x5
            local rating  = gametype == "3x3" and s.rating3x3  or s.rating5x5
            local leaves  = gametype == "3x3" and s.leaves3x3  or s.leaves5x5
            local leavepct = gametype == "3x3" and s.leaves3x3_percent or s.leaves5x5_percent

            -- bnproxy stats output format (DO NOT MODIFY)
            pvpgn.chat.send_whisper(session.account_name, nil,
                string.format("[%s] %s DotA (%s): [%s] %d pts. Leave count: %d (%.1f%%)",
                    s.country, u, gametype, rank, rating, leaves, leavepct))
        end
    else
        -- Show stats for the target player
        pvpgn.chat.send_whisper(session.account_name, nil,
            string.format("[%s] ", stats.country) ..
            pvpgn.i18n.localize(session.account_name, "{}'s record:", target_acc.name))
        pvpgn.chat.send_whisper(session.account_name, nil,
            pvpgn.i18n.localize(session.account_name, "DotA games") ..
            string.format(" (%s): %d-%d [%s] %d %s",
                "5x5", stats.wins5x5, stats.losses5x5, stats.rank5x5, stats.rating5x5, pts))
        pvpgn.chat.send_whisper(session.account_name, nil,
            pvpgn.i18n.localize(session.account_name, "DotA games") ..
            string.format(" (%s): %d-%d [%s] %d %s",
                "3x3", stats.wins3x3, stats.losses3x3, stats.rank3x3, stats.rating3x3, pts))

        -- Show streaks only for self-lookup
        if not target_name then
            local win  = pvpgn.i18n.localize(session.account_name, "win")
            local loss = pvpgn.i18n.localize(session.account_name, "loss")
            local s5   = stats.streaks5x5 >= 0 and win or loss
            local s3   = stats.streaks3x3 >= 0 and win or loss
            pvpgn.chat.send_whisper(session.account_name, nil,
                pvpgn.i18n.localize(session.account_name, "Current {} streak", "5x5") ..
                string.format(" (%s): %s", s5, stats.streaks5x5))
            pvpgn.chat.send_whisper(session.account_name, nil,
                pvpgn.i18n.localize(session.account_name, "Current {} streak", "3x3") ..
                string.format(" (%s): %s", s3, stats.streaks3x3))
        end

        pvpgn.chat.send_whisper(session.account_name, nil,
            pvpgn.i18n.localize(session.account_name, "Current leave count:") ..
            string.format(" %d (%s%%)", stats.leaves, stats.leaves_percent))
    end

    return 0
end

return M
