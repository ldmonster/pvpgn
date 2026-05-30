--[[
    GHost++ user commands — /host, /chost, /unhost, /swap, /open, /close,
    /start, /abort, /pub, /priv, /ghost (callback dispatcher).

    Migrated from scripts/lua/ghost/command.lua
    Copyright (C) 2014 HarpyWar (harpywar@gmail.com)
    This file is a part of the PvPGN Project http://pvpgn.pro
    Licensed under the same terms as Lua itself.
]]--

local helper = require("helper")
local ghost  = require("ghost")

-- Warcraft III Expansion client tag
local CLIENTTAG_WAR3XP = "W3XP"

-- Game type constant (all types)
local GAME_TYPE_ALL = 0

local M = {}

-- Helper: split command text into up to max_args tokens
-- args[0] = command name, args[1..n] = arguments
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

-- Helper: guard — return 1 (pass-through) if ghost is disabled or wrong client
local function guard_w3xp(session)
    if session.clienttag ~= CLIENTTAG_WAR3XP then return 1 end
    return nil
end

-- /host [mode] [type] [gamename]
function M.cmd_host(session, args)
    if guard_w3xp(session) then return 1 end

    if not args[1] or not args[2] or not args[3] then
        pvpgn.commands.describe(session.account_name, "/host")
        return -1
    end

    -- If user already has a mapped bot, check if the game still exists
    if helper.get_userbot_name(session.account_name) then
        local gamename = helper.get_userbot_game(session.account_name)
        local game = pvpgn.game.find_by_name(gamename, session.clienttag, GAME_TYPE_ALL)
        if game and next(game) then
            pvpgn.chat.send_whisper(session.account_name, nil,
                pvpgn.i18n.localize(session.account_name,
                    "You already host a game \"{}\". Use /unhost to destroy it.", gamename))
            return -1
        else
            helper.del_userbot(session.account_name)
        end
    end

    local botname = helper.select_bot(session.account_name)
    if not botname then
        pvpgn.chat.send_whisper(session.account_name, nil,
            pvpgn.i18n.localize(session.account_name,
                "Enable to create game. HostBots are temporary offline."))
        return -1
    end

    helper.message_send(botname,
        string.format("/pvpgn host %s %s %s %s",
            session.account_name, args[1], args[2], args[3]))
    return 0
end

-- /chost [code] [gamename]
function M.cmd_chost(session, args)
    if guard_w3xp(session) then return 1 end

    if not args[1] or not args[2] then
        pvpgn.commands.describe(session.account_name, "/chost")
        -- List available maps
        for _, map in ipairs(ghost.maplist) do
            pvpgn.chat.send_whisper(session.account_name, nil,
                string.format("%s = %s", map.code, map.name))
        end
        return -1
    end

    -- Find map by code
    local mapfile = nil
    for _, map in ipairs(ghost.maplist) do
        if args[1]:lower() == map.code:lower() then
            mapfile = map.filename
            break
        end
    end

    if not mapfile then
        pvpgn.chat.send_whisper(session.account_name, nil,
            pvpgn.i18n.localize(session.account_name, "Invalid map code."))
        return -1
    end

    -- Check for existing hosted game
    if helper.get_userbot_name(session.account_name) then
        local gamename = helper.get_userbot_game(session.account_name)
        local game = pvpgn.game.find_by_name(gamename, session.clienttag, GAME_TYPE_ALL)
        if game and next(game) then
            pvpgn.chat.send_whisper(session.account_name, nil,
                pvpgn.i18n.localize(session.account_name,
                    "You already host a game \"{}\". Use /unhost to destroy it.", gamename))
            return -1
        else
            helper.del_userbot(session.account_name)
        end
    end

    local botname = helper.select_bot(session.account_name)
    if not botname then
        pvpgn.chat.send_whisper(session.account_name, nil,
            pvpgn.i18n.localize(session.account_name,
                "Enable to create game. HostBots are temporary offline."))
        return -1
    end

    helper.message_send(botname,
        string.format("/pvpgn chost %s %s %s",
            session.account_name, mapfile, args[2]))
    return 0
end

-- /unhost
function M.cmd_unhost(session, args)
    if guard_w3xp(session) then return 1 end

    local botname = helper.get_userbot_name(session.account_name)
    if not botname then
        pvpgn.chat.send_whisper(session.account_name, nil,
            pvpgn.i18n.localize(session.account_name, "You don't host a game."))
        return -1
    end

    -- Do not allow unhost if the game has already started
    local game = pvpgn.game.find_by_id(session.game_id)
    if game and next(game) and game.status == "started" and game.owner == botname then
        pvpgn.chat.send_whisper(session.account_name, nil,
            pvpgn.i18n.localize(session.account_name, "You can't unhost a started game."))
        return -1
    end

    helper.message_send(botname,
        string.format("/pvpgn unhost %s", session.account_name))
    helper.del_userbot(session.account_name)

    pvpgn.chat.send_whisper(session.account_name, nil,
        pvpgn.i18n.localize(session.account_name, "Your game was destroyed."))
    return 0
end

-- /swap [slot1] [slot2]
function M.cmd_swap(session, args)
    if guard_w3xp(session) then return 1 end
    if not helper.is_owner(pvpgn.account.find_by_name(session.account_name)) then return 1 end

    if not args[1] or not args[2] then
        pvpgn.commands.describe(session.account_name, "/swap")
        return -1
    end

    local botname = helper.get_userbot_name(session.account_name)
    helper.message_send(botname,
        string.format("/pvpgn swap %s %s %s", session.account_name, args[1], args[2]))
    return 0
end

-- /open [slot] and /close [slot]
function M.cmd_open_close(session, args)
    if guard_w3xp(session) then return 1 end
    if not helper.is_owner(pvpgn.account.find_by_name(session.account_name)) then return 1 end

    if not args[1] then
        pvpgn.commands.describe(session.account_name, args[0] or "/open")
        return -1
    end

    local botname = helper.get_userbot_name(session.account_name)
    helper.message_send(botname,
        string.format("/pvpgn %s %s %s", args[0] or "open", session.account_name, args[1]))
    return 0
end

-- /start, /abort, /pub, /priv
function M.cmd_start_abort_pub_priv(session, args)
    if guard_w3xp(session) then return 1 end
    if not helper.is_owner(pvpgn.account.find_by_name(session.account_name)) then return 1 end

    local botname = helper.get_userbot_name(session.account_name)
    helper.message_send(botname,
        string.format("/pvpgn %s %s", args[0] or "start", session.account_name))
    return 0
end

return M
