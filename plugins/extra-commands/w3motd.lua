--[[
    /w3motd command — read w3motd.txt line by line and send to user.
    Only available to Warcraft III clients.

    Migrated from scripts/lua/command/w3motd.lua
    Copyright (C) 2014 HarpyWar (harpywar@gmail.com)
    This file is a part of the PvPGN Project http://pvpgn.pro
    Licensed under the same terms as Lua itself.
]]--

-- Warcraft III client tag constants (same values as v1 CLIENTTAG_* globals)
local CLIENTTAG_WAR3XP     = "W3XP"
local CLIENTTAG_WARCRAFT3  = "WAR3"

local M = {}

-- /w3motd
function M.handle(session, args)
    -- allow Warcraft III clients only
    local tag = session.clienttag
    if not (tag == CLIENTTAG_WAR3XP or tag == CLIENTTAG_WARCRAFT3) then
        return 1  -- pass through to next handler
    end

    -- resolve motd file path via plugin config
    local motdfile = pvpgn.config.get("motdw3file") or "motdw3.txt"
    local path = pvpgn.fs.vardir() .. motdfile

    if not pvpgn.fs.exists(path) then
        pvpgn.log("warn", "w3motd: file not found: " .. path)
        return 0
    end

    -- read file and send each line to the user
    local content = pvpgn.fs.read(path)
    if content then
        for line in content:gmatch("[^\r\n]+") do
            pvpgn.chat.send_whisper(session.account_name, session.account_name, line)
        end
    end

    return 0
end

return M
