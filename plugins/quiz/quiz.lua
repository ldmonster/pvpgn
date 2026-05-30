--[[
    Quiz core logic — game state, question flow, hint system, scoring.

    Migrated from scripts/lua/quiz/quiz.lua
    Copyright (C) 2014 HarpyWar (harpywar@gmail.com)
    This file is a part of the PvPGN Project http://pvpgn.pro
    Licensed under the same terms as Lua itself.
]]--

local helper  = require("helper")
local records = require("records")

local M = {}

-- ── Game state ────────────────────────────────────────────────────────────────

-- Dictionary loaded from the question file: array of { question, word }
M.dictionary = {}
-- Per-game player scores: array of { username, points }
M.records_current = {}
-- Per-game point diffs (for display): table username → delta
M.records_diff = {}

-- Active channel name (nil when no quiz is running)
M.active_channel = nil

-- Internal counters / state
local _question_counter        = 0
local _hint_counter            = 0
local _current_index           = 0
local _hint                    = nil
local _time_start              = 0
local _streak                  = { username = nil, count = 0 }
local _next_question_skip_first = false
local _unanswered              = true

-- ── Config helpers ────────────────────────────────────────────────────────────

local function cfg_max_questions()  return tonumber(pvpgn.config.get("max_questions"))  or 100 end
local function cfg_question_delay() return tonumber(pvpgn.config.get("question_delay")) or 5   end
local function cfg_hint_delay()     return tonumber(pvpgn.config.get("hint_delay"))     or 20  end
local function cfg_competitive()    return pvpgn.config.get("competitive_mode") ~= "false"     end
local function cfg_users_in_top()   return tonumber(pvpgn.config.get("users_in_top"))   or 15  end

-- ── Internal helpers ──────────────────────────────────────────────────────────

-- Send a message to the active quiz channel
local function ch_send(text)
    if M.active_channel then
        pvpgn.chat.send_channel(M.active_channel, text)
    end
end

-- Find (or create) a player entry in records_current; return index
local function current_find(username, prevent_new)
    for i, t in ipairs(M.records_current) do
        if t.username == username then return i end
    end
    if prevent_new then return nil end
    table.insert(M.records_current, { username = username, points = 0 })
    return #M.records_current
end

-- Parse a "question = answer" line from a question file
local function parse_qa_line(line)
    local q, a = line:match("^%s*(.-)%s*=%s*(.-)%s*$")
    return q, a
end

-- Load question dictionary from a .txt file
local function load_dictionary(filename)
    M.dictionary = {}
    local content = pvpgn.fs.read(filename)
    if not content then return false end
    for line in content:gmatch("[^\r\n]+") do
        -- skip comment/header lines (start with * or blank)
        if not line:match("^%s*%*") and not line:match("^%s*$") then
            local q, a = parse_qa_line(line)
            if q and a then
                table.insert(M.dictionary, { question = q, word = a })
            end
        end
    end
    return #M.dictionary > 0
end

-- ── Public API ────────────────────────────────────────────────────────────────

-- Start a quiz in the given channel using the named question file
function M.start(channelname, quizname)
    -- Resolve path: questions/ subdirectory relative to plugin root
    local filename = pvpgn.fs.plugin_dir() .. "/questions/" .. quizname:lower() .. ".txt"

    if not pvpgn.fs.exists(filename) then
        pvpgn.chat.send_channel(channelname, "Quiz file not found: " .. filename)
        return false
    end

    M.active_channel = channelname

    -- Reset state
    _question_counter = 0
    _streak.username  = nil
    _streak.count     = 0
    M.dictionary      = {}
    M.records_current = {}
    M.records_diff    = {}

    -- Load persistent records (cached after first load)
    records.load()

    -- Load question dictionary
    if load_dictionary(filename) then
        ch_send(string.format('Quiz "%s" started!', quizname))
        M.next_question()
        return true
    end
    return false
end

-- Stop the current quiz (optionally with the name of who stopped it)
function M.stop(stopped_by)
    pvpgn.timer.remove("q_hint")
    pvpgn.timer.remove("q_question")

    local msg = stopped_by and ("Quiz stopped by " .. stopped_by) or "Quiz finished!"
    ch_send(msg)

    -- Display current-game scores
    if next(M.records_current) then
        table.sort(M.records_current, helper.compare_desc)
        ch_send("Records of this game:")
        for i, t in ipairs(M.records_current) do
            ch_send(string.format("  %d. %s [%d points]", i, t.username, t.points))
        end
    end

    -- Merge current scores into total records
    if next(records.records_total) then
        for _, t in ipairs(M.records_current) do
            -- Competitive mode: steal half points from the displaced player
            if cfg_competitive() then
                local idx_total = records.total_find(t.username, true)
                if idx_total and records.records_total[idx_total] and
                   records.records_total[idx_total].username ~= t.username then
                    local steal = math.floor(t.points / 2)
                    M.records_diff[records.records_total[idx_total].username] = steal * -1
                    records.records_total[idx_total].points =
                        math.max(0, records.records_total[idx_total].points - steal)
                end
            end

            local idx = records.total_find(t.username)
            local tusername = records.records_total[idx].username
            M.records_diff[tusername] = t.points

            if records.records_total[idx].points == 0 then
                records.records_total[idx].points = t.points
            else
                if (M.records_diff[tusername] or 0) < 0 then
                    M.records_diff[tusername] = (M.records_diff[tusername] or 0) + t.points
                end
                records.records_total[idx].points = records.records_total[idx].points + t.points
            end
        end

        table.sort(records.records_total, helper.compare_desc)
        M.display_top_players()
    else
        -- First ever game — seed total from current
        records.records_total = M.records_current
    end

    records.save()
    M.active_channel = nil
end

-- Advance to the next question (or end the quiz if max reached)
function M.next_question()
    _unanswered = true
    _question_counter = _question_counter + 1

    if _question_counter > cfg_max_questions() then
        M.stop()
        return
    end

    pvpgn.timer.remove("q_hint")
    _next_question_skip_first = false
    pvpgn.timer.add("q_question", cfg_question_delay(), M.tick_next_question)
end

-- Timer callback: display the next question
function M.tick_next_question()
    -- Skip the first tick (delay before showing question)
    if not _next_question_skip_first then
        _next_question_skip_first = true
        return
    end

    pvpgn.timer.remove("q_question")

    _time_start    = os.clock()
    _current_index = math.random(#M.dictionary)
    local q        = M.dictionary[_current_index]

    ch_send("--------------------------------------------------------")
    ch_send(string.format(" %s (%d letters)", q.question, #q.word))

    _hint         = helper.hide_answer(q.word)
    _hint_counter = 0

    pvpgn.timer.add("q_hint", cfg_hint_delay(), M.tick_hint_answer)
    _unanswered = false
end

-- Timer callback: reveal one more hint character
function M.tick_hint_answer()
    -- Skip first tick
    if _hint_counter == 0 then
        _hint_counter = _hint_counter + 1
        return
    end
    _hint_counter = _hint_counter + 1

    local q = M.dictionary[_current_index]
    _hint = helper.show_next_symbol(_hint, q.word)

    if _hint ~= q.word then
        ch_send("Hint: " .. _hint)
    else
        -- All letters revealed — nobody answered
        _hint_counter = 0
        M.no_answer()
    end
end

-- Nobody answered — show the answer and move on
function M.no_answer()
    ch_send("Nobody answered. The answer was: " .. _hint)
    if _streak.count > 0 then
        _streak.count = _streak.count - 1
    end
    M.next_question()
end

-- Handle a channel message as a potential quiz answer
function M.handle_message(username, text)
    if _unanswered then return end

    local q = M.dictionary[_current_index]
    if not q then return end

    if q.word:upper() == text:upper() then
        local time_diff = os.clock() - _time_start

        -- Calculate points
        local points = 1 + (#q.word - _hint_counter) - math.floor(time_diff / cfg_hint_delay())
        if points <= 0 then points = 1 end

        -- Streak tracking
        local prev_streak = { username = _streak.username, count = _streak.count }
        if _streak.username == username then
            _streak.count = _streak.count + 1
        else
            _streak.username = username
            _streak.count    = 0
        end

        local bonus = ""
        if _streak.count > 0 then
            points = points + _streak.count
            bonus  = string.format(" +%d streak bonus", _streak.count)
        end

        local idx   = current_find(username)
        local total = M.records_current[idx].points + points
        M.records_current[idx].points = total

        -- Competitive mode: previous leader loses half of winner's total
        if cfg_competitive() and prev_streak.username and prev_streak.username ~= username then
            local lose_points = math.floor(total / 2)
            local pidx = current_find(prev_streak.username)
            M.records_current[pidx].points =
                math.max(0, M.records_current[pidx].points - lose_points)
        end

        ch_send(string.format(
            "%s is correct! The answer is: %s (+%d points%s, %d total) [%d sec]",
            username, q.word, (points - _streak.count), bonus, total, math.floor(time_diff)
        ))

        M.next_question()
    end
end

-- Display top players to the active channel
function M.display_top_players()
    local top = cfg_users_in_top()
    ch_send("Top " .. top .. " Quiz players:")
    for i, t in ipairs(records.records_total) do
        if i > top then break end
        local diff = ""
        local d = M.records_diff[t.username]
        if d then
            if d < 0 then
                diff = "(" .. d .. ")"
            elseif d > 0 then
                diff = "(+" .. d .. ")"
            end
        end
        ch_send(string.format("  %d. %s [%d points] %s", i, t.username, t.points, diff))
    end
end

return M
