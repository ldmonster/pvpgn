--[[
    GHost++ helper utilities — bot/user mapping, ping tracking, bot selection.

    Migrated from scripts/lua/ghost/helper.lua
    Copyright (C) 2014 HarpyWar (harpywar@gmail.com)
    This file is a part of the PvPGN Project http://pvpgn.pro
    Licensed under the same terms as Lua itself.
]]--

local M = {}

-- table: username → { bot, game, gametype, time }
M.user2bot = {}

-- array of usernames who should not receive a /ping response (silent mode)
local silentpings = {}

-- ── User→Bot mapping ──────────────────────────────────────────────────────────

-- Return the bot name mapped to a user (nil if none)
function M.get_userbot_name(username)
    if not next(M.user2bot) or not M.user2bot[username] then return nil end
    return M.user2bot[username].bot
end

-- Return the game name created by the user's mapped bot
function M.get_userbot_game(username)
    if not next(M.user2bot) or not M.user2bot[username] then return nil end
    return M.user2bot[username].game
end

-- Return the game type for the user's mapped bot game
function M.get_userbot_gametype(username)
    if not next(M.user2bot) or not M.user2bot[username] then return nil end
    return M.user2bot[username].gametype
end

-- Find the username who owns a game by game name; return false if not found
function M.find_userbot_by_game(gamename)
    for u, bot in pairs(M.user2bot) do
        if bot.game == gamename then return u end
    end
    return false
end

-- Map a user to a bot+game
function M.set_userbot(username, botname, gamename, gametype)
    M.user2bot[username] = { bot = botname, game = gamename, gametype = gametype, time = os.time() }
end

-- Remove a user's bot mapping
function M.del_userbot(username)
    M.user2bot[username] = nil
end

-- ── State persistence ─────────────────────────────────────────────────────────

-- Save user→bot table to a file in vardir
function M.save_userbots()
    local filename = pvpgn.fs.vardir() .. "ghost_users.dmp"
    local lines = {}
    for u, bot in pairs(M.user2bot) do
        -- Serialize as "username = bot|game|gametype|time"
        table.insert(lines, string.format("%s = %s|%s|%s|%d",
            u, bot.bot, bot.game or "", bot.gametype or "", bot.time or 0))
    end
    pvpgn.fs.write(filename, table.concat(lines, "\n") .. "\n")
    pvpgn.log("debug", "Saved GHost state to " .. filename)
end

-- Load user→bot table from a file in vardir
function M.load_userbots()
    local filename = pvpgn.fs.vardir() .. "ghost_users.dmp"
    if not pvpgn.fs.exists(filename) then return end

    local content = pvpgn.fs.read(filename)
    if not content then return end

    M.user2bot = {}
    for line in content:gmatch("[^\r\n]+") do
        if not line:match("^%s*$") then
            local u, rest = line:match("^%s*(.-)%s*=%s*(.-)%s*$")
            if u and rest then
                local bot, game, gametype, t = rest:match("^([^|]*)|([^|]*)|([^|]*)|(%d*)$")
                M.user2bot[u] = {
                    bot      = bot or rest,
                    game     = game or "",
                    gametype = gametype or "",
                    time     = tonumber(t) or 0
                }
            end
        end
    end

    if next(M.user2bot) then
        pvpgn.log("debug", "Loaded GHost state from " .. filename)
        for u, bot in pairs(M.user2bot) do
            pvpgn.log("debug", string.format("  %s → %s (%s, %d)", u, bot.bot, bot.game, bot.time))
        end
    end
end

-- ── Silent ping flag ──────────────────────────────────────────────────────────

-- Mark a user as silent (suppress next ping response)
function M.set_silent(username)
    table.insert(silentpings, username)
end

-- Return true and remove the flag if the user is marked silent
function M.get_silent(username)
    for k, v in pairs(silentpings) do
        if v == username then
            table.remove(silentpings, k)
            return true
        end
    end
    return false
end

-- ── Bot utilities ─────────────────────────────────────────────────────────────

-- Return true if username is in the configured bot list
function M.is_bot(username)
    local bots = pvpgn.config.get("bots")
    if type(bots) == "table" then
        for _, bot in ipairs(bots) do
            if bot:lower() == username:lower() then return true end
        end
    elseif type(bots) == "string" then
        for bot in bots:gmatch("[^,]+") do
            if bot:match("^%s*(.-)%s*$"):lower() == username:lower() then return true end
        end
    end
    return false
end

-- Return true if the account is the owner of the bot in their current game
function M.is_owner(account)
    if not account.game_id then return false end
    local game = pvpgn.game.find_by_id(account.game_id)
    if not game then return false end
    return game.owner == M.get_userbot_name(account.name)
end

-- Send a whisper from the server to a bot (used to relay commands)
function M.message_send(botname, text)
    pvpgn.chat.send_whisper(botname, nil, text)
end

-- Select the best available bot for a user (by stored ping; fallback to first online)
function M.select_bot(username)
    local bots_cfg = pvpgn.config.get("bots") or {}
    local bot_list = {}
    if type(bots_cfg) == "table" then
        bot_list = bots_cfg
    else
        for b in tostring(bots_cfg):gmatch("[^,]+") do
            table.insert(bot_list, b:match("^%s*(.-)%s*$"))
        end
    end

    -- Try to find a bot the user has no ping for yet
    local pings = pvpgn.db.get_botping(username) or {}
    local botname = nil
    for _, bot in ipairs(bot_list) do
        local found = false
        for _, p in ipairs(pings) do
            if p.bot == bot then found = true; break end
        end
        if not found then botname = bot; break end
    end

    -- If all bots have pings, pick the one with the lowest ping
    if not botname and #pings > 0 then
        table.sort(pings, function(a, b) return (tonumber(a.ping) or 9999) < (tonumber(b.ping) or 9999) end)
        botname = pings[1].bot
    end

    -- Verify the chosen bot is online; fall back to first online bot
    if botname then
        local acc = pvpgn.account.find_by_name(botname)
        if not acc or not acc.online then botname = nil end
    end

    if not botname then
        for _, bot in ipairs(bot_list) do
            local acc = pvpgn.account.find_by_name(bot)
            if acc and acc.online then return bot end
        end
        return nil
    end

    return botname
end

-- Load custom map list from resources/maplist.txt
function M.load_maps()
    local mapfile = pvpgn.fs.plugin_dir() .. "/resources/maplist.txt"
    if not pvpgn.fs.exists(mapfile) then
        pvpgn.log("warn", "GHost maplist not found: " .. mapfile)
        return {}
    end

    local maps = {}
    local content = pvpgn.fs.read(mapfile)
    if content then
        for line in content:gmatch("[^\r\n]+") do
            -- skip comment/header lines
            if not line:match("^%s*%*") and not line:match("^%s*$") then
                local code, rest = line:match("^%s*(.-)%s*=%s*(.-)%s*$")
                if code and rest then
                    -- rest is "MapName|filename.w3x"
                    local mapname, mapfile_name = rest:match("^(.-)%s*|%s*(.-)%s*$")
                    if mapname and mapfile_name then
                        table.insert(maps, { code = code, name = mapname, filename = mapfile_name })
                    end
                end
            end
        end
    end
    pvpgn.log("debug", "Loaded " .. #maps .. " GHost custom maps")
    return maps
end

return M
