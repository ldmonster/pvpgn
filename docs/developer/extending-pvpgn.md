# Extending PvPGN via Plugins

PvPGN v3 exposes a stable **Lua Plugin API v2** (`pvpgn.*` namespace) that lets
you add commands, react to server events, and integrate third-party services
without touching the C++ core.

---

## Quick Start

1. Create a directory under `plugins/` with a `plugin.toml` manifest and a
   `main.lua` entry point.
2. Restart `bnetd` (or send `SIGHUP` to reload plugins at runtime).
3. Your plugin is live.

```
plugins/
└── my-plugin/
    ├── plugin.toml   ← manifest
    └── main.lua      ← entry point (required)
```

---

## Plugin Manifest (`plugin.toml`)

Every plugin must have a `plugin.toml` at its root.

```toml
[plugin]
id          = "com.example.my-plugin"   # reverse-DNS, globally unique
name        = "My Plugin"
version     = "1.0.0"                   # semver
description = "Does something useful."
author      = "Your Name"
license     = "MIT"
entry_point = "main.lua"
api_version_req = ">=3.0.0"            # minimum PvPGN API version required
provides    = ["my-plugin.feature"]    # capabilities this plugin exports
conflicts   = []                       # plugin IDs that must NOT be loaded

[config]
# Arbitrary key/value pairs accessible via pvpgn.config.get()
greeting = "Hello, world!"

[[dependencies]]
plugin_id   = "com.pvpgn.core.chat"
version_req = ">=3.0.0"
optional    = false
```

### Manifest fields

| Field | Required | Description |
|-------|----------|-------------|
| `id` | ✅ | Globally unique reverse-DNS identifier |
| `name` | ✅ | Human-readable display name |
| `version` | ✅ | Semantic version string |
| `entry_point` | ✅ | Lua file to load (relative to plugin directory) |
| `api_version_req` | ✅ | Minimum `pvpgn.*` API version (semver range) |
| `description` | — | Short description |
| `author` | — | Author name or email |
| `license` | — | SPDX license identifier |
| `provides` | — | Capability tokens exported to other plugins |
| `conflicts` | — | Plugin IDs that must not be co-loaded |

---

## Entry Point (`main.lua`)

The entry point must define two global functions:

```lua
-- Called once when the plugin is loaded (or reloaded).
function init()
    -- register commands, subscribe to events, start timers …
end

-- Called once when the plugin is unloaded (server shutdown or hot-reload).
function shutdown()
    -- cancel timers, release resources …
end
```

---

## The `pvpgn.*` Namespace (API v2)

All server functionality is exposed through the global `pvpgn` table.
The full reference is in [`docs/developer/lua-api-v2.md`](lua-api-v2.md).

### Logging

```lua
pvpgn.log("info",  "Plugin started")
pvpgn.log("warn",  "Config value missing, using default")
pvpgn.log("error", "Something went wrong")
pvpgn.log("debug", "Verbose diagnostic message")
```

### Registering Commands — `pvpgn.commands.register`

```lua
pvpgn.commands.register("/hello", function(session, args)
    pvpgn.send_chat(session.username, "Hello, " .. session.username .. "!")
end)
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `command` | string | Command string including the leading `/` |
| `handler` | function | Called with `(session, args)` when a user types the command |

The `session` table passed to the handler contains:

| Field | Type | Description |
|-------|------|-------------|
| `username` | string | Account name of the user who issued the command |
| `account_id` | integer | Internal account ID |
| `channel` | string | Current channel name, or `nil` if not in a channel |

`args` is a string containing everything after the command name (may be empty).

### Subscribing to Events — `pvpgn.events.on`

```lua
pvpgn.events.on("user_login", function(event)
    pvpgn.log("info", event.username .. " logged in from " .. event.ip)
end)
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `event_name` | string | Name of the event to subscribe to |
| `handler` | function | Called with an event table when the event fires |

#### Available Events

| Event | Payload fields | Description |
|-------|---------------|-------------|
| `user_login` | `username`, `ip`, `client_tag` | User authenticated successfully |
| `user_logout` | `username` | User disconnected |
| `user_join_channel` | `username`, `channel` | User joined a channel |
| `user_leave_channel` | `username`, `channel` | User left a channel |
| `game_created` | `game_id`, `game_name`, `owner` | A new game lobby was created |
| `game_started` | `game_id` | A game transitioned from lobby to in-progress |
| `game_ended` | `game_id` | A game ended |
| `client_readmemory` | `username`, `address`, `data` | Memory-read response from a game client |
| `chat_message` | `username`, `channel`, `text` | A chat message was sent in a channel |

### Sending Chat Messages

```lua
pvpgn.send_chat(username, message)
```

Sends a whisper-style message to the named user.

### Reading Plugin Config

```lua
local greeting = pvpgn.config.get("greeting")
```

Returns the string value of a key from the `[config]` section of
`plugin.toml`, or `nil` if the key does not exist.

### Timers

```lua
-- Add a repeating timer
pvpgn.timer.add("my_timer", interval_seconds, function()
    -- called every interval_seconds
end)

-- Remove a timer
pvpgn.timer.remove("my_timer")
```

---

## Example Plugin

The following minimal plugin greets users when they log in and registers a
`/ping` command.

**`plugins/hello-world/plugin.toml`**

```toml
[plugin]
id          = "com.example.hello-world"
name        = "Hello World"
version     = "1.0.0"
description = "Greets users and provides a /ping command."
author      = "Example Author"
license     = "MIT"
entry_point = "main.lua"
api_version_req = ">=3.0.0"
provides    = []
conflicts   = []
```

**`plugins/hello-world/main.lua`**

```lua
-- Hello World plugin for PvPGN v3
-- Demonstrates pvpgn.commands.register and pvpgn.events.on

function init()
    pvpgn.log("info", "Hello World plugin loading…")

    -- Register a /ping command
    pvpgn.commands.register("/ping", function(session, args)
        pvpgn.send_chat(session.username, "Pong!")
    end)

    -- Greet users on login
    pvpgn.events.on("user_login", function(event)
        pvpgn.send_chat(event.username, "Welcome to the server, " .. event.username .. "!")
    end)

    pvpgn.log("info", "Hello World plugin ready")
end

function shutdown()
    pvpgn.log("info", "Hello World plugin unloaded")
end
```

---

## Existing Plugins

The `plugins/` directory ships several ready-to-use plugins:

| Plugin | Directory | Description |
|--------|-----------|-------------|
| Starcraft Anti-Hack | `plugins/antihack-starcraft/` | Memory-scan anti-maphack for SC:BW 1.16.1 |
| Example Quiz | `plugins/example-quiz/` | Trivia quiz game with native C extension |
| Extra Commands | `plugins/extra-commands/` | Additional chat commands (`/redirect`, `/w3motd`) |
| Ghost Bot | `plugins/ghost/` | GHost++ bot integration |
| Quiz | `plugins/quiz/` | Full-featured quiz with per-game question sets |

---

## Plugin Versioning

Plugins declare a `version` (semver) and an `api_version_req` (semver range).
The server rejects plugins whose `api_version_req` is not satisfied by the
running PvPGN API version.

See [`docs/developer/plugin-versioning-guide.md`](plugin-versioning-guide.md)
for the full versioning policy, including how to handle breaking API changes
and how to publish a plugin to the community registry.

---

## Testing Plugins

Run the Lua API conformance test suite to verify your plugin does not rely on
removed or renamed API functions:

```sh
cmake --preset v3-dev
cmake --build --preset v3-dev --target lua_api_v2_conformance
ctest --preset v3-dev -R lua_api_v2
```

See [`docs/developer/testing.md`](testing.md) for the full testing guide.

---

## Further Reading

- [`docs/developer/lua-api-v2.md`](lua-api-v2.md) — Complete `pvpgn.*` function reference
- [`docs/developer/plugin-versioning-guide.md`](plugin-versioning-guide.md) — Versioning policy
- [`docs/developer/sandbox-integration-guide.md`](sandbox-integration-guide.md) — Sandboxing and security model
- [`plans/11-plugin-and-extensibility.md`](../../plans/11-plugin-and-extensibility.md) — Design rationale
