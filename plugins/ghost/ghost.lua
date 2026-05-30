--[[
    GHost++ core — plugin load/unload and map list management.

    Migrated from scripts/lua/ghost/ghost.lua
    Copyright (C) 2014 HarpyWar (harpywar@gmail.com)
    This file is a part of the PvPGN Project http://pvpgn.pro
    Licensed under the same terms as Lua itself.
]]--

local helper = require("helper")

local M = {}

-- Custom map list loaded from resources/maplist.txt
M.maplist = {}

-- Load plugin state: restore user→bot table and preload map list
function M.load()
    -- Restore persisted user→bot assignments
    helper.load_userbots()

    -- Preload custom map list
    M.maplist = helper.load_maps()
end

-- Save plugin state: persist user→bot table
function M.unload()
    helper.save_userbots()
end

return M
