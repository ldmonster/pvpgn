-- Lua API v2 (pvpgn.* namespace)
--[[
    GHost++ Integration Plugin for PvPGN v3

    Provides full GHost++ bot integration for Warcraft III game hosting:
      - /host [mode] [type] [gamename]   — create a hosted game via a bot
      - /chost [code] [gamename]         — create a game from the custom map list
      - /unhost                          — destroy your hosted game
      - /swap [slot1] [slot2]            — swap player slots
      - /open [slot] / /close [slot]     — open or close a slot
      - /start / /abort / /pub / /priv   — game control commands
      - /ghost [cmd] [code] {args}       — bot→pvpgn callback dispatcher
      - /ping                            — show latency to the GHost bot
      - /stats [username]                — show DotA statistics

    Migrated from:
      scripts/lua/ghost/
      scripts/lua/command/ping.lua
      scripts/lua/command/stats.lua

    Uses the pvpgn.* Lua API v2 namespace.
]]--

local ghost    = require("ghost")
local command  = require("command")
local callback = require("command_callback")
local handle   = require("handle")
local ping     = require("ping")
local stats    = require("stats")

-- Plugin initialization
function init()
    pvpgn.log("info", "GHost++ Integration plugin initializing...")

    -- Load persisted state and map list
    ghost.load()

    -- ── User → GHost commands ────────────────────────────────────────────────

    pvpgn.commands.register("/host", function(session, args)
        return command.cmd_host(session, args)
    end, {
        group = "users",
        description = "Create a hosted game: /host <mode> <type> <gamename>"
    })

    pvpgn.commands.register("/chost", function(session, args)
        return command.cmd_chost(session, args)
    end, {
        group = "users",
        description = "Create a game from the custom map list: /chost <code> <gamename>"
    })

    pvpgn.commands.register("/unhost", function(session, args)
        return command.cmd_unhost(session, args)
    end, {
        group = "users",
        description = "Destroy your hosted game"
    })

    pvpgn.commands.register("/swap", function(session, args)
        return command.cmd_swap(session, args)
    end, {
        group = "users",
        description = "Swap two player slots: /swap <slot1> <slot2>"
    })

    pvpgn.commands.register("/open", function(session, args)
        args[0] = "/open"
        return command.cmd_open_close(session, args)
    end, {
        group = "users",
        description = "Open a player slot: /open <slot>"
    })

    pvpgn.commands.register("/close", function(session, args)
        args[0] = "/close"
        return command.cmd_open_close(session, args)
    end, {
        group = "users",
        description = "Close a player slot: /close <slot>"
    })

    pvpgn.commands.register("/start", function(session, args)
        args[0] = "/start"
        return command.cmd_start_abort_pub_priv(session, args)
    end, { group = "users", description = "Start the hosted game" })

    pvpgn.commands.register("/abort", function(session, args)
        args[0] = "/abort"
        return command.cmd_start_abort_pub_priv(session, args)
    end, { group = "users", description = "Abort the hosted game" })

    pvpgn.commands.register("/pub", function(session, args)
        args[0] = "/pub"
        return command.cmd_start_abort_pub_priv(session, args)
    end, { group = "users", description = "Make the hosted game public" })

    pvpgn.commands.register("/priv", function(session, args)
        args[0] = "/priv"
        return command.cmd_start_abort_pub_priv(session, args)
    end, { group = "users", description = "Make the hosted game private" })

    -- ── GHost → PvPGN callback ───────────────────────────────────────────────

    pvpgn.commands.register("/ghost", function(session, args)
        return callback.cmd_ghost(session, args)
    end, {
        group = "bots",
        description = "GHost bot callback dispatcher (internal use)"
    })

    -- ── Ping and Stats ───────────────────────────────────────────────────────

    pvpgn.commands.register("/ping", function(session, args)
        return ping.handle(session, args)
    end, {
        group = "users",
        description = "Show your latency to the GHost bot (W3XP only)"
    })

    pvpgn.commands.register("/stats", function(session, args)
        return stats.handle(session, args)
    end, {
        group = "users",
        description = "Show DotA statistics: /stats [username]"
    })

    -- ── Event subscriptions ──────────────────────────────────────────────────

    pvpgn.events.on("game_user_joined", function(event)
        handle.handle_game_userjoin(event)
    end)

    pvpgn.events.on("user_logged_in", function(event)
        handle.handle_user_login(event)
    end)

    pvpgn.log("info", "GHost++ Integration plugin initialized")
end

-- Plugin shutdown — persist state
function shutdown()
    ghost.unload()
    pvpgn.log("info", "GHost++ Integration plugin unloaded")
end
