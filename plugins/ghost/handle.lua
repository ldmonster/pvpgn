--[[
    GHost++ event handlers — game user join, user login.

    Migrated from scripts/lua/ghost/handle.lua
    Copyright (C) 2014 HarpyWar (harpywar@gmail.com)
    This file is a part of the PvPGN Project http://pvpgn.pro
    Licensed under the same terms as Lua itself.
]]--

local helper = require("helper")
local ping   = require("ping")
local stats  = require("stats")

local M = {}

-- Called when a user joins a GHost-hosted game
function M.handle_game_userjoin(event)
    local game    = event.game
    local account = event.account

    -- Only handle games owned by a GHost bot
    if not helper.is_bot(game.owner) then return end

    -- Send ping silently (to record it without showing to user)
    helper.set_silent(account.name)
    local botaccount = pvpgn.account.find_by_name(game.owner)
    if botaccount then
        ping.handle({ account = botaccount, raw_text = "/ping" })
    end

    -- If DotA server mode is enabled, show stats for all players
    if pvpgn.config.get("dota_server") ~= "false" then
        local owner = helper.find_userbot_by_game(game.name)
        -- Only show stats for ladder games (gametype is set)
        if not owner or helper.get_userbot_gametype(owner) == "" then return end

        -- Show stats of the joining player to each existing player
        if game.players then
            for u in game.players:gmatch("[^,]+") do
                u = u:match("^%s*(.-)%s*$")
                local useracc = pvpgn.account.find_by_name(u)
                if useracc then
                    stats.handle({ account = useracc, raw_text = "/stats " .. account.name })
                end
                -- Show stats of each existing player to the joining player
                stats.handle({ account = account, raw_text = "/stats " .. u })
            end
        end

        pvpgn.chat.send_whisper(account.name, nil,
            pvpgn.i18n.localize(account.name,
                "Joined ladder game. Game owner: {}", owner))
    end
end

-- Called when a user logs in
function M.handle_user_login(event)
    local account = event.account

    -- If the logged-in user is a GHost bot, activate pvpgn mode on the bot side
    if helper.is_bot(account.name) then
        helper.message_send(account.name, "/pvpgn init")
        pvpgn.log("debug", "init bot " .. account.name)
    end
end

return M
