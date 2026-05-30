--[[
    Quiz helper utilities.

    Migrated from scripts/lua/quiz/helper.lua
    Copyright (C) 2014 HarpyWar (harpywar@gmail.com)
    This file is a part of the PvPGN Project http://pvpgn.pro
    Licensed under the same terms as Lua itself.
]]--

local M = {}

-- Replace each non-space character in a string with '*'
-- Example: "hello world" → "***** *****"
function M.hide_answer(input)
    local output = input
    for i = 1, #input do
        local c = input:sub(i, i)
        if c ~= " " then
            output = output:sub(1, i-1) .. "*" .. output:sub(i+1)
        end
    end
    return output
end

-- Reveal one random hidden ('*') character from the original string
-- Example: hidden="***** *****", original="hello world" → "***l* *****"
function M.show_next_symbol(hidden, original)
    if hidden == original then
        return hidden
    end

    local replaced = false
    local output = hidden
    while not replaced do
        local i = math.random(#hidden)
        local c = hidden:sub(i, i)
        if c == "*" then
            local c2 = original:sub(i, i)
            output = output:sub(1, i-1) .. c2 .. output:sub(i+1)
            replaced = true
        end
    end
    return output
end

-- Comparator: sort records table descending by points
function M.compare_desc(a, b)
    return tonumber(a.points) > tonumber(b.points)
end

return M
