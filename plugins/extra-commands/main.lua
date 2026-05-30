-- Lua API v2 (pvpgn.* namespace)
--[[
    Extra Commands Plugin for PvPGN v3

    Provides two standalone utility commands:
      /redirect <username> <message>  — send a server message directly to a user
      /w3motd                         — show Warcraft III MOTD file to the caller

    Migrated from:
      scripts/lua/command/redirect.lua
      scripts/lua/command/w3motd.lua

    Uses the pvpgn.* Lua API v2 namespace.
]]--

local redirect = require("redirect")
local w3motd   = require("w3motd")

-- Plugin initialization
function init()
    pvpgn.log("info", "Extra Commands plugin initializing...")

    -- Register /redirect command
    pvpgn.commands.register("/redirect", function(session, args)
        return redirect.handle(session, args)
    end, {
        group = "users",
        description = "Send a server message directly to another user: /redirect <username> <message>"
    })

    -- Register /w3motd command
    pvpgn.commands.register("/w3motd", function(session, args)
        return w3motd.handle(session, args)
    end, {
        group = "users",
        description = "Show the Warcraft III message of the day (W3 clients only)"
    })

    pvpgn.log("info", "Extra Commands plugin initialized (commands: /redirect, /w3motd)")
end

-- Plugin shutdown (nothing to clean up)
function shutdown()
    pvpgn.log("info", "Extra Commands plugin unloaded")
end
