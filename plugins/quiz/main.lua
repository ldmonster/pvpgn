-- Lua API v2 (pvpgn.* namespace)
--[[
    Quiz Plugin for PvPGN v3

    Channel quiz game with leaderboard and competitive mode.
    Players answer questions in a channel; points are tracked across sessions.

    Commands:
      /quiz start <dictionary>  — start a quiz in the current channel
      /quiz stop                — stop the running quiz
      /quiz stats               — show top leaderboard
      /quiz stats <username>    — show a player's record

    Migrated from scripts/lua/quiz/

    Uses the pvpgn.* Lua API v2 namespace.
]]--

local quiz    = require("quiz")
local command = require("command")

-- Plugin initialization
function init()
    pvpgn.log("info", "Quiz plugin initializing...")

    -- Register /quiz command
    pvpgn.commands.register("/quiz", function(session, args)
        return command.handle(session, args)
    end, {
        group = "users",
        description = "Channel quiz game: /quiz start <dict> | stop | stats [user]"
    })

    -- Subscribe to channel message events to check for quiz answers
    pvpgn.events.on("channel_message_sent", function(event)
        -- Only process messages in the active quiz channel
        if quiz.active_channel and event.channel_name == quiz.active_channel then
            quiz.handle_message(event.account_name, event.message)
        end
    end)

    pvpgn.log("info", "Quiz plugin initialized (command: /quiz)")
end

-- Plugin shutdown — stop any running quiz cleanly
function shutdown()
    if quiz.active_channel then
        quiz.stop(nil)
    end
    pvpgn.log("info", "Quiz plugin unloaded")
end
