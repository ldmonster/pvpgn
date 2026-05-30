-- Lua API v2 (pvpgn.* namespace)
--[[
    Starcraft Anti-Hack Plugin for PvPGN v3

    Periodically scans memory of all Starcraft: Brood War players in active
    games to detect maphack tools. Detected cheaters are locked and kicked.

    Migrated from scripts/lua/antihack/starcraft.lua

    Uses the pvpgn.* Lua API v2 namespace.
]]--

local starcraft = require("starcraft")

-- Plugin initialization
function init()
    pvpgn.log("info", "Starcraft Anti-Hack plugin initializing...")

    -- Read check interval from plugin config (default: 60 seconds)
    local interval = tonumber(pvpgn.config.get("check_interval")) or 60

    -- Register periodic timer to scan all SC:BW players
    pvpgn.timer.add("ah_sc_timer", interval, function()
        starcraft.timer_tick()
    end)

    -- Subscribe to client memory-read response events
    pvpgn.events.on("client_readmemory", function(event)
        starcraft.handle_readmemory(event)
    end)

    pvpgn.log("info", "Starcraft Anti-Hack plugin initialized (interval: " .. interval .. "s)")
end

-- Plugin shutdown — remove the timer
function shutdown()
    pvpgn.timer.remove("ah_sc_timer")
    pvpgn.log("info", "Starcraft Anti-Hack plugin unloaded")
end
