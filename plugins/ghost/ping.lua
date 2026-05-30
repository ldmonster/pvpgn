--[[
    /ping command — relay ping request to the GHost bot owning the current game.
    Only active for Warcraft III Expansion clients in a GHost-hosted game.

    Migrated from scripts/lua/command/ping.lua
    Copyright (C) 2014 HarpyWar (harpywar@gmail.com)
    This file is a part of the PvPGN Project http://pvpgn.pro
    Licensed under the same terms as Lua itself.
]]--

local helper = require("helper")

-- Warcraft III Expansion client tag
local CLIENTTAG_WAR3XP = "W3XP"

local M = {}

-- Handle /ping command (or internal call from handle.lua)
-- session may be a real command session or a synthetic event table
function M.handle(session, args)
    -- Only for W3XP clients
    if session.clienttag and session.clienttag ~= CLIENTTAG_WAR3XP then
        return 1  -- pass through
    end

    -- User must be in a game
    if not session.game_id then return 1 end

    local game = pvpgn.game.find_by_id(session.game_id)
    if not game or not next(game) then return 1 end

    -- Game must be owned by a GHost bot
    if not helper.is_bot(game.owner) then return 1 end

    -- Relay ping request to the bot
    helper.message_send(game.owner,
        string.format("/pvpgn ping %s", session.account_name or session.name))

    return 0
end

return M
