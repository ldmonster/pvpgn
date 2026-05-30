--[[
    /redirect command — send a message to another user from the server.
    Works like /announce but directly to a user and message text is not red.

    Migrated from scripts/lua/command/redirect.lua
    Copyright (C) 2014 HarpyWar (harpywar@gmail.com)
    This file is a part of the PvPGN Project http://pvpgn.pro
    Licensed under the same terms as Lua itself.
]]--

local M = {}

-- /redirect <username> <message>
function M.handle(session, args)
    -- args[1] = destination username, args[2] = message text
    local dest_name = args[1]
    local message   = args[2]

    if not dest_name or not message then
        pvpgn.commands.describe(session.account_name, "/redirect")
        return -1
    end

    -- look up destination account
    local dest = pvpgn.account.find_by_name(dest_name)
    if not dest or not dest.online then
        pvpgn.chat.send_whisper(
            session.account_name,
            session.account_name,
            pvpgn.i18n.localize(session.account_name, "User \"{}\" is offline", dest_name)
        )
        return -1
    end

    -- deliver the message
    pvpgn.chat.send_whisper(dest.name, dest.name, message)

    return 0
end

return M
