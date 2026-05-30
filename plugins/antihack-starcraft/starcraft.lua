--[[
    Simple Anti MapHack for Starcraft: BroodWar 1.16.1

    Migrated from scripts/lua/antihack/starcraft.lua
    Copyright (C) 2014 HarpyWar (harpywar@gmail.com)
    This file is a part of the PvPGN Project http://pvpgn.pro
    Licensed under the same terms as Lua itself.
]]--

-- Starcraft: Brood War client tag constant
local CLIENTTAG_BROODWARS = "SEXP"

-- Unique request ID for maphack memory scan
local MH_REQUEST_ID = 99
-- Memory offset for map-visible flag
local MH_OFFSET     = 0x0047FD12
-- Expected value at that offset when no maphack is active
local MH_VALUE      = 139

local M = {}

-- Helper: convert a 2-byte little-endian byte string to an integer
local function bytes_to_int(data, offset, len)
    -- pvpgn.client.read_memory returns raw bytes; interpret as little-endian uint
    local value = 0
    for i = 0, len - 1 do
        local byte = data:byte(offset + i + 1) or 0
        value = value + byte * (256 ^ i)
    end
    return value
end

-- Send memory-read requests to all SC:BW players currently in games
function M.timer_tick()
    for _, game in pairs(pvpgn.server.games()) do
        -- check only Starcraft: Brood War games
        if game.clienttag == CLIENTTAG_BROODWARS then
            -- check only games with at least one player
            if game.players and game.players ~= "" then
                for username in game.players:gmatch("[^,]+") do
                    username = username:match("^%s*(.-)%s*$")  -- trim whitespace
                    pvpgn.client.read_memory(username, MH_REQUEST_ID, MH_OFFSET, 2)
                end
            end
        end
    end
end

-- Handle the memory-read response from a client
function M.handle_readmemory(event)
    local account    = event.account
    local request_id = event.request_id
    local data       = event.data

    local is_cheater = false

    if request_id == MH_REQUEST_ID then
        local value = bytes_to_int(data, 0, 2)
        if value ~= MH_VALUE then
            is_cheater = true
        end
    end

    if is_cheater then
        -- lock the cheater's account
        pvpgn.moderation.lock_account(account.name, "we do not like cheaters")

        -- notify all players in the same game
        local game = pvpgn.game.find_by_id(account.game_id)
        if game and game.players then
            for username in game.players:gmatch("[^,]+") do
                username = username:match("^%s*(.-)%s*$")
                pvpgn.chat.send_channel(
                    username,
                    account.name .. " was banned by the antihack system."
                )
            end
        end

        -- kick the cheater
        pvpgn.moderation.kick_connection(account.name)

        pvpgn.log("info", account.name .. " was banned by the antihack system.")
    end
end

return M
