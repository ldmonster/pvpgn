--[[
    Quiz command handlers — /quiz start|stop|stats

    Migrated from scripts/lua/quiz/command.lua
    Copyright (C) 2014 HarpyWar (harpywar@gmail.com)
    This file is a part of the PvPGN Project http://pvpgn.pro
    Licensed under the same terms as Lua itself.
]]--

local quiz    = require("quiz")
local records = require("records")

local M = {}

-- Helper: split a command string into tokens (up to max_args extra tokens)
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

-- Helper: check if account has operator or admin privileges
local function is_operator_or_admin(account_name)
    local acc = pvpgn.account.find_by_name(account_name)
    if not acc then return false end
    return acc.is_operator or acc.is_admin
end

-- /quiz <start|stop|stats> [args]
function M.handle(session, args)
    local sub = args[1]

    if sub == "start" then
        return M.cmd_start(session, args[2])
    elseif sub == "stop" then
        return M.cmd_stop(session)
    elseif sub == "stats" then
        if not args[2] then
            return M.cmd_toplist(session)
        else
            return M.cmd_stats(session, args[2])
        end
    end

    pvpgn.commands.describe(session.account_name, "/quiz")
    return -1
end

-- /quiz start [dictionary]
function M.cmd_start(session, filename)
    if not is_operator_or_admin(session.account_name) then
        pvpgn.chat.send_whisper(
            session.account_name, session.account_name,
            pvpgn.i18n.localize(session.account_name,
                "You must be at least a Channel Operator to use this command.")
        )
        return -1
    end

    local channel = pvpgn.channel.get(session.channel_id)
    if not channel then
        pvpgn.chat.send_whisper(
            session.account_name, session.account_name,
            pvpgn.i18n.localize(session.account_name,
                "This command can only be used inside a channel.")
        )
        return -1
    end

    if quiz.active_channel then
        pvpgn.chat.send_whisper(
            session.account_name, session.account_name,
            pvpgn.i18n.localize(session.account_name,
                "Quiz has already ran in channel \"{}\". Use /quiz stop to force finish.",
                quiz.active_channel)
        )
        return -1
    end

    -- Validate dictionary file
    local filelist = pvpgn.config.get("quiz_filelist") or "misc, dota, warcraft"
    if not filename then
        pvpgn.chat.send_whisper(
            session.account_name, session.account_name,
            pvpgn.i18n.localize(session.account_name, "Available Quiz dictionaries: ")
        )
        pvpgn.chat.send_whisper(session.account_name, session.account_name, "   " .. filelist)
        return -1
    end

    local qfile = pvpgn.fs.plugin_dir() .. "/questions/" .. filename:lower() .. ".txt"
    if not pvpgn.fs.exists(qfile) then
        pvpgn.chat.send_whisper(
            session.account_name, session.account_name,
            pvpgn.i18n.localize(session.account_name, "Available Quiz dictionaries: ")
        )
        pvpgn.chat.send_whisper(session.account_name, session.account_name, "   " .. filelist)
        return -1
    end

    quiz.start(channel.name, filename)
    return 0
end

-- /quiz stop
function M.cmd_stop(session)
    if not is_operator_or_admin(session.account_name) then
        pvpgn.chat.send_whisper(
            session.account_name, session.account_name,
            pvpgn.i18n.localize(session.account_name,
                "You must be at least a Channel Operator to use this command.")
        )
        return -1
    end

    if not quiz.active_channel then
        pvpgn.chat.send_whisper(
            session.account_name, session.account_name,
            pvpgn.i18n.localize(session.account_name, "Quiz is not running.")
        )
        return -1
    end

    quiz.stop(session.account_name)
    return 0
end

-- /quiz stats  (top list)
function M.cmd_toplist(session)
    if not records.load() then
        return -1
    end

    local top = tonumber(pvpgn.config.get("users_in_top")) or 15
    pvpgn.chat.send_whisper(
        session.account_name, session.account_name,
        pvpgn.i18n.localize(session.account_name, "Top {} Quiz records:", top)
    )

    for i, t in ipairs(records.records_total) do
        if i > top then break end
        pvpgn.chat.send_whisper(
            session.account_name, session.account_name,
            string.format("  %d. %s [%d %s]", i, t.username, t.points,
                pvpgn.i18n.localize(session.account_name, "points"))
        )
    end

    return 0
end

-- /quiz stats <username>
function M.cmd_stats(session, username)
    if not records.load() then
        return -1
    end

    local found = false
    for i, t in ipairs(records.records_total) do
        if t.username:upper() == username:upper() then
            pvpgn.chat.send_whisper(
                session.account_name, session.account_name,
                pvpgn.i18n.localize(session.account_name, "{}'s Quiz record:", t.username)
            )
            pvpgn.chat.send_whisper(
                session.account_name, session.account_name,
                string.format("  %d. %s [%d %s]", i, t.username, t.points,
                    pvpgn.i18n.localize(session.account_name, "points"))
            )
            found = true
        end
    end

    if not found then
        pvpgn.chat.send_whisper(
            session.account_name, session.account_name,
            pvpgn.i18n.localize(session.account_name, "{} has never played Quiz.", username)
        )
    end

    return 0
end

return M
