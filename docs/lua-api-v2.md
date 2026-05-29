# PvPGN Lua API v2 Reference

> **Introduced in R349** — This document describes the `pvpgn.*` Lua namespace
> available to plugins running in PvPGN v3.

---

## Overview

The Lua API v2 exposes server functionality to plugins through a single `pvpgn`
table.  All functions are registered by `pvpgn::infra::scripting::register_v2_api()`
and delegate to C++ handlers provided by the composition root.

```lua
-- Every plugin has access to the pvpgn table:
pvpgn.log("info", "Hello from my plugin!")
```

---

## Function Reference

### `pvpgn.log(level, message)`

Log a message through the server's logging subsystem.

| Parameter | Type   | Description |
|-----------|--------|-------------|
| `level`   | string | Severity: `"debug"`, `"info"`, `"warn"`, or `"error"` |
| `message` | string | UTF-8 message text |

**Returns:** nothing

**Example:**
```lua
pvpgn.log("info", "Plugin started")
pvpgn.log("warn", "Config value missing, using default")
pvpgn.log("error", "Failed to connect to database")
```

---

### `pvpgn.send_chat(username, message)`

Send a chat message (whisper) to a specific user.

| Parameter  | Type   | Description |
|------------|--------|-------------|
| `username` | string | Target account name |
| `message`  | string | Message text to send |

**Returns:** nothing

**Example:**
```lua
pvpgn.send_chat("Alice", "Welcome back!")
pvpgn.send_chat(session.account_id, "Your score is: " .. score)
```

---

### `pvpgn.get_account(username)`

Retrieve account information for a user.

| Parameter  | Type   | Description |
|------------|--------|-------------|
| `username` | string | Account name to look up |

**Returns:** A table with the following fields, or `nil` if the account does not exist:

| Field    | Type    | Description |
|----------|---------|-------------|
| `name`   | string  | Account name |
| `flags`  | integer | Account flags bitmask |
| `wins`   | integer | Total wins |
| `losses` | integer | Total losses |

**Example:**
```lua
local account = pvpgn.get_account("Alice")
if account then
    pvpgn.log("info", "Alice has " .. account.wins .. " wins")
else
    pvpgn.log("warn", "Account 'Alice' not found")
end
```

---

### `pvpgn.ban_account(username, reason)`

Ban an account from the server.

| Parameter  | Type   | Description |
|------------|--------|-------------|
| `username` | string | Account name to ban |
| `reason`   | string | Human-readable ban reason |

**Returns:** nothing

**Example:**
```lua
pvpgn.ban_account("Cheater123", "Detected speed hack")
```

---

### `pvpgn.kick_user(username, reason)`

Disconnect a currently-connected user.

| Parameter  | Type   | Description |
|------------|--------|-------------|
| `username` | string | Account name to kick |
| `reason`   | string | Human-readable kick reason |

**Returns:** nothing

**Example:**
```lua
pvpgn.kick_user("Spammer", "Excessive chat spam")
```

---

### `pvpgn.broadcast(channel, message)`

Broadcast a message to all users in a channel (or all users if `channel` is empty).

| Parameter  | Type   | Description |
|------------|--------|-------------|
| `channel`  | string | Channel name, or `""` for server-wide broadcast |
| `message`  | string | Message text |

**Returns:** nothing

**Example:**
```lua
pvpgn.broadcast("", "Server restart in 5 minutes!")
pvpgn.broadcast("Diablo", "Ladder reset in 1 hour")
```

---

## Migration Guide: `bnetd_*` → `pvpgn.*`

Plugins written for older PvPGN versions used `bnetd_*` global functions.
The legacy compatibility shim maps these to the new `pvpgn.*` API automatically.

### Migration Table

| Old API (`bnetd_*`)                    | New API (`pvpgn.*`)                      |
|----------------------------------------|------------------------------------------|
| `bnetd_send_message(user, msg)`        | `pvpgn.send_chat(user, msg)`             |
| `bnetd_get_account_info(user)`         | `pvpgn.get_account(user)`                |
| `bnetd_ban_user(user, reason)`         | `pvpgn.ban_account(user, reason)`        |
| `bnetd_kick_user(user, reason)`        | `pvpgn.kick_user(user, reason)`          |
| `bnetd_broadcast(channel, msg)`        | `pvpgn.broadcast(channel, msg)`          |
| `bnetd_log(level, msg)`               | `pvpgn.log(level, msg)`                  |

### Loading the Legacy Shim

The shim is installed automatically by the server when loading plugins.
To load it manually in a test environment:

```lua
-- Option 1: The server calls install_legacy_shim() from C++ before loading your plugin.
-- Option 2: In your plugin, check if bnetd_* globals exist before using them.

if bnetd_send_message then
    -- Legacy API available
    bnetd_send_message("Alice", "Hello!")
else
    -- Use new API
    pvpgn.send_chat("Alice", "Hello!")
end
```

### Migrating Your Plugin

Replace all `bnetd_*` calls with their `pvpgn.*` equivalents:

```lua
-- Before (legacy API):
bnetd_send_message(account_id, "Welcome!")
local info = bnetd_get_account_info(account_id)
bnetd_log("info", "Plugin started")

-- After (API v2):
pvpgn.send_chat(account_id, "Welcome!")
local info = pvpgn.get_account(account_id)
pvpgn.log("info", "Plugin started")
```

---

## Complete Plugin Example

```lua
-- Lua API v2 (pvpgn.* namespace)
-- my-plugin/main.lua

function init()
    pvpgn.log("info", "My plugin initializing...")
    
    -- Register a command
    pvpgn.commands.register("/hello", function(session, args)
        local account = pvpgn.get_account(session.account_id)
        if account then
            pvpgn.send_chat(session.account_id,
                "Hello, " .. account.name .. "! You have " .. account.wins .. " wins.")
        end
    end, { group = "users", description = "Say hello" })
    
    pvpgn.log("info", "My plugin ready")
end

-- Event handler: called when a user logs in
function on_user_login(payload)
    -- payload is a JSON string: {"username": "Alice"}
    pvpgn.broadcast("", "Welcome, " .. payload .. "!")
    return ""  -- return empty JSON response
end
```

---

## Error Handling

Lua errors in plugin callbacks are caught by the C++ host and logged as errors.
The server continues running even if a plugin callback throws.

```lua
function on_user_login(payload)
    -- This error will be caught and logged; the server won't crash.
    error("Something went wrong!")
end
```

---

## See Also

- [`docs/sandbox-integration-guide.md`](sandbox-integration-guide.md) — Plugin sandboxing
- [`docs/plugin-versioning-guide.md`](plugin-versioning-guide.md) — Plugin versioning
- [`src/v3/application/ports/include/application/ports/script_host.hpp`](../src/v3/application/ports/include/application/ports/script_host.hpp) — C++ IScriptHost port
- [`src/v3/infra/scripting/include/infra/scripting/lua_api_v2.hpp`](../src/v3/infra/scripting/include/infra/scripting/lua_api_v2.hpp) — C++ registration function
- [`plugins/example-quiz/main.lua`](../plugins/example-quiz/main.lua) — Full plugin example
