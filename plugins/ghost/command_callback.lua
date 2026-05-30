--[[
    GHost++ callback dispatcher — handles /ghost commands sent by bots back to PvPGN.

    Migrated from scripts/lua/ghost/command_callback.lua
    Copyright (C) 2014 HarpyWar (harpywar@gmail.com)
    This file is a part of the PvPGN Project http://pvpgn.pro
    Licensed under the same terms as Lua itself.
]]--

local helper = require("helper")

local M = {}

-- Game status constant
local GAME_STATUS_STARTED = "started"

-- Helper: split command text into up to max_args tokens
local function split_command(text, max_args)
    local args = {}
    local i = 0
    for token in text:gmatch("%S+") do
        args[i] = token
        i = i + 1
        if max_args > 0 and i > max_args then break end
    end
    return args
end

-- /ghost [cmd] [code] {args}
-- Only accepted from authorized bot accounts.
function M.cmd_ghost(session, args)
    -- Restrict to authorized bots only
    if not helper.is_bot(session.account_name) then return 1 end

    local cmd  = args[1]
    local code = args[2]

    if code ~= "ok" then
        -- code == "err": args[3] contains the error message
        -- HINT: handle error here if needed
        return -1
    end

    if cmd == "init" then
        pvpgn.log("info", "GHost bot is connected (" .. session.account_name .. ")")

    elseif cmd == "gameresult" then
        M.callback_gameresult(args[3], args[4], args[5])

    elseif cmd == "host" then
        -- Re-split to support game names with spaces
        local full_args = split_command(session.raw_text, 4)
        M.callback_host(full_args[3], session.account_name, full_args[4])

    elseif cmd == "chost" then
        local full_args = split_command(session.raw_text, 4)
        M.callback_chost(full_args[3], session.account_name, full_args[4])

    elseif cmd == "unhost" then
        M.callback_unhost(args[3], args[4])

    elseif cmd == "ping" then
        M.callback_ping(args[3], args[4], args[5])

    elseif cmd == "swap" or cmd == "open" or cmd == "close"
        or cmd == "start" or cmd == "abort" or cmd == "pub" or cmd == "priv" then
        -- HINT: notify user about success if desired
    end

    return 0
end

-- Bot confirmed game creation via /host
function M.callback_host(username, botname, gamename)
    local gametype = "5x5"
    if gamename and gamename:find("3x3") then gametype = "3x3" end
    helper.set_userbot(username, botname, gamename, gametype)
    pvpgn.chat.send_whisper(username, nil,
        pvpgn.i18n.localize(username,
            "Game \"{}\" is created by {}. You are an owner.", gamename, botname))
end

-- Bot confirmed game creation via /chost
function M.callback_chost(username, botname, gamename)
    helper.set_userbot(username, botname, gamename, "")
    pvpgn.chat.send_whisper(username, nil,
        pvpgn.i18n.localize(username,
            "Game \"{}\" is created by {}. You are an owner.", gamename, botname))
end

-- Bot confirmed game destruction
function M.callback_unhost(username, gamename)
    helper.del_userbot(username)
    pvpgn.chat.send_whisper(username, nil,
        pvpgn.i18n.localize(username, "The game \"{}\" was destroyed.", gamename))
end

-- Bot sent ping results for players in a game
function M.callback_ping(username, players_str, pings_str)
    local account = pvpgn.account.find_by_name(username)
    if not account then return end

    local game = pvpgn.game.find_by_id(account.game_id)
    if not game or not next(game) then return end
    if not helper.is_bot(game.owner) then return end

    local silent = helper.get_silent(username)
    local ms     = pvpgn.i18n.localize(username, "ms")

    -- Build users table from parallel comma-separated lists
    local users = {}
    local i = 1
    for v in players_str:gmatch("[^,]+") do
        users[i] = { name = v:match("^%s*(.-)%s*$") }
        i = i + 1
    end
    i = 1
    for v in pings_str:gmatch("[^,]+") do
        if users[i] then users[i].ping = v:match("^%s*(.-)%s*$") end
        i = i + 1
    end

    local latency_all = ""
    for _, u in ipairs(users) do
        -- Update stored ping for this bot
        local pings = pvpgn.db.get_botping(u.name) or {}
        local found = false
        for k, p in ipairs(pings) do
            if p.bot == game.owner then
                pings[k].ping = u.ping
                found = true
                break
            end
        end
        if not found then
            table.insert(pings, { date = os.time(), bot = game.owner, ping = u.ping })
        end
        pvpgn.db.set_botping(u.name, pings)

        latency_all = latency_all .. string.format("%s: [%s %s]; ", u.name, u.ping, ms)

        if game.status == GAME_STATUS_STARTED and not silent then
            pvpgn.chat.send_whisper(username, nil,
                pvpgn.i18n.localize(username,
                    "{}'s latency to {}: {} {}", u.name, game.owner, u.ping, ms))
        end
    end

    if game.status ~= GAME_STATUS_STARTED and not silent then
        pvpgn.chat.send_whisper(username, nil,
            pvpgn.i18n.localize(username, "Latency: {}", latency_all))
    end
end

-- Bot sent game result (DotA stats update)
function M.callback_gameresult(players_str, ratings_str, results_str)
    local users = {}
    local i = 1
    for v in players_str:gmatch("[^,]+") do
        users[i] = { name = v:match("^%s*(.-)%s*$") }
        i = i + 1
    end
    i = 1
    for v in ratings_str:gmatch("[^,]+") do
        if users[i] then users[i].rating = tonumber(v) end
        i = i + 1
    end
    i = 1
    for v in results_str:gmatch("[^,]+") do
        if users[i] then users[i].result = tonumber(v) end
        i = i + 1
    end

    local count = #users
    if count ~= 6 and count ~= 10 then
        pvpgn.log("error", string.format(
            "Bad game result (%s %s %s)", players_str, ratings_str, results_str))
        return
    end

    local gametype = (count == 6) and "3x3" or "5x5"

    for _, u in ipairs(users) do
        local rating, wins, losses, leaves, streaks

        if gametype == "3x3" then
            rating  = pvpgn.db.get_dotarating_3x3(u.name)
            wins    = pvpgn.db.get_dotawins_3x3(u.name)
            losses  = pvpgn.db.get_dotalosses_3x3(u.name)
            leaves  = pvpgn.db.get_dotaleaves_3x3(u.name)
            streaks = pvpgn.db.get_dotastreaks_3x3(u.name)
        else
            rating  = pvpgn.db.get_dotarating_5x5(u.name)
            wins    = pvpgn.db.get_dotawins_5x5(u.name)
            losses  = pvpgn.db.get_dotalosses_5x5(u.name)
            leaves  = pvpgn.db.get_dotaleaves_5x5(u.name)
            streaks = pvpgn.db.get_dotastreaks_5x5(u.name)
        end

        -- Update win/loss/leave counters
        local result = u.result
        if result == 2 then      -- leave + win
            leaves = leaves + 1; wins = wins + 1
        elseif result == 1 then  -- win
            wins = wins + 1
        elseif result == 0 then  -- loss
            losses = losses + 1
        elseif result == -1 then -- leave + loss
            leaves = leaves + 1; losses = losses + 1
        end

        -- Update streak
        local result_str
        if streaks >= 0 then
            if result > 0 then
                streaks = streaks + 1
            else
                streaks = -1
            end
            result_str = pvpgn.i18n.localize(u.name, "won")
        else
            if result <= 0 then
                streaks = streaks - 1
            else
                streaks = 1
            end
            result_str = pvpgn.i18n.localize(u.name, "lose")
        end

        -- Persist updated stats
        if gametype == "3x3" then
            pvpgn.db.set_dotarating_3x3(u.name, u.rating)
            pvpgn.db.set_dotawins_3x3(u.name, wins)
            pvpgn.db.set_dotalosses_3x3(u.name, losses)
            pvpgn.db.set_dotaleaves_3x3(u.name, leaves)
            pvpgn.db.set_dotastreaks_3x3(u.name, streaks)
        else
            pvpgn.db.set_dotarating_5x5(u.name, u.rating)
            pvpgn.db.set_dotawins_5x5(u.name, wins)
            pvpgn.db.set_dotalosses_5x5(u.name, losses)
            pvpgn.db.set_dotaleaves_5x5(u.name, leaves)
            pvpgn.db.set_dotastreaks_5x5(u.name, streaks)
        end

        local points = u.rating - rating
        pvpgn.chat.send_whisper(u.name, nil,
            pvpgn.i18n.localize(u.name, "You {} {} points.", result_str, points))
        pvpgn.chat.send_whisper(u.name, nil,
            pvpgn.i18n.localize(u.name,
                "New DotA ({}) rating: {} pts. Current {} streak: {}",
                gametype, u.rating, result_str, streaks))
    end
end

return M
