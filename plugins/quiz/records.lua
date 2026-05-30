--[[
    Quiz records persistence — load/save total player scores.

    Migrated from scripts/lua/quiz/records.lua
    Copyright (C) 2014 HarpyWar (harpywar@gmail.com)
    This file is a part of the PvPGN Project http://pvpgn.pro
    Licensed under the same terms as Lua itself.
]]--

local M = {}

-- In-memory cache of total records: array of { username, points }
M.records_total = {}

-- Parse a "key = value" line from the records file
local function parse_kv_line(line)
    local k, v = line:match("^%s*(.-)%s*=%s*(.-)%s*$")
    return k, v
end

-- Load total records from persistent store (quiz_records.txt in vardir)
-- Returns true on success, false if file not found or empty
function M.load()
    local filename = pvpgn.fs.vardir() .. "quiz_records.txt"
    if not pvpgn.fs.exists(filename) then
        pvpgn.log("debug", "Quiz records file not found: " .. filename)
        return false
    end

    -- Only reload if cache is empty
    if next(M.records_total) then
        return true
    end

    local content = pvpgn.fs.read(filename)
    if not content then
        return false
    end

    for line in content:gmatch("[^\r\n]+") do
        -- skip comment/header lines
        if not line:match("^%s*%*") and not line:match("^%s*$") then
            local username, points = parse_kv_line(line)
            if username and points then
                table.insert(M.records_total, { username = username, points = tonumber(points) or 0 })
            end
        end
    end

    return true
end

-- Save total records to persistent store
function M.save()
    local filename = pvpgn.fs.vardir() .. "quiz_records.txt"
    local lines = {}
    for _, t in ipairs(M.records_total) do
        table.insert(lines, t.username .. " = " .. tostring(t.points))
    end
    pvpgn.fs.write(filename, table.concat(lines, "\n") .. "\n")
end

-- Find username in records_total; insert with 0 points if not found (unless prevent_new)
function M.total_find(username, prevent_new)
    for i, t in ipairs(M.records_total) do
        if t.username == username then return i end
    end
    if prevent_new then return nil end
    table.insert(M.records_total, { username = username, points = 0 })
    return #M.records_total
end

return M
