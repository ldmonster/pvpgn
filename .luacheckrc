-- .luacheckrc — luacheck configuration for PvPGN v3
-- Run: luacheck plugins/ scripts/lua/
-- See: https://luacheck.readthedocs.io/en/stable/config.html

-- Target Lua 5.3 (minimum supported by PvPGN v3)
std = "lua53"

-- Globals injected by the PvPGN C++ host into every plugin's Lua state.
-- These are NOT defined in Lua source; luacheck must be told they exist.
globals = {
    -- Plugin API v2 root table (pvpgn.* namespace)
    "pvpgn",

    -- Plugin lifecycle hooks (defined by each plugin's main.lua)
    "init",
    "shutdown",
}

-- Read-only globals (standard Lua + common libraries)
read_globals = {
    "require",
    "print",
    "tostring",
    "tonumber",
    "type",
    "pairs",
    "ipairs",
    "next",
    "select",
    "unpack",
    "table",
    "string",
    "math",
    "io",
    "os",
    "pcall",
    "xpcall",
    "error",
    "assert",
    "setmetatable",
    "getmetatable",
    "rawget",
    "rawset",
    "rawequal",
    "rawlen",
    "load",
    "loadfile",
    "dofile",
    "collectgarbage",
    "coroutine",
    "utf8",
}

-- Ignore unused loop variable warnings for conventional names
unused_args = true
-- Allow shadowing of variables in nested scopes (common in Lua idioms)
allow_defined = true
-- Warn on unused variables (but not loop variables named "_")
unused = true

-- Per-directory overrides
files["plugins/"] = {
    -- Plugins may define additional globals for their own sub-modules
    allow_defined_top = true,
}

files["scripts/lua/"] = {
    -- Boot/utility scripts may define globals consumed by other scripts
    allow_defined_top = true,
    globals = {
        -- Legacy boot globals still used by scripts/lua/main.lua
        "handle_channel",
        "handle_client",
        "handle_command",
        "handle_game",
        "handle_server",
        "handle_user",
    },
}

-- Exclude vendored or generated files
exclude_files = {
    "vendor/",
    "build/",
}
