# PvPGN v3 Plugin System

This directory contains plugins for PvPGN v3. Plugins extend server functionality through a stable, sandboxed API.

## Plugin Structure

Each plugin is a directory containing:

```
plugin-name/
├── plugin.toml          # Plugin manifest
├── main.lua             # Entry point (for Lua plugins)
├── lib.so               # Entry point (for native C++ plugins)
└── resources/           # Optional: assets, data files, etc.
```

## Plugin Manifest (plugin.toml)

```toml
name        = "plugin-name"
version     = "1.0.0"
authors     = ["Your Name"]
api         = "3.0"
entry       = "main.lua"
description = "Plugin description"

capabilities = [
    "chat.send",
    "commands.register",
    "events.subscribe",
    "db.read",
    "store.read",
    "store.write"
]
```

### Capabilities

Plugins must declare required capabilities:

- **chat.send** - Send channel messages and whispers
- **chat.emote** - Send emotes
- **commands.register** - Register custom commands
- **events.subscribe** - Subscribe to domain events
- **events.publish** - Publish custom events
- **db.read** - Read from database (accounts, channels, etc.)
- **db.write** - Write to database
- **fs.read** - Read files
- **fs.write** - Write files
- **net.http** - Make HTTP requests
- **net.socket** - Raw socket access
- **moderation.ban** - Ban accounts/IPs
- **moderation.kick** - Kick connections
- **store.read** - Read plugin-local key-value store
- **store.write** - Write plugin-local key-value store
- **admin.reload_config** - Reload server config
- **admin.shutdown** - Shutdown server

## Lua Plugin API

### Account API

```lua
-- Find account by name
local account = pvpgn.account.find_by_name("PlayerName")
if account then
    print(account.name)
    print(account.email)
end

-- List accounts
local accounts = pvpgn.account.list({page=1, pageSize=50})
```

### Chat API

```lua
-- Send channel message
pvpgn.chat.send_channel(channel_id, "Hello everyone!")

-- Send whisper
pvpgn.chat.send_whisper(from_id, to_id, "Private message")

-- Send emote
pvpgn.chat.emote(account_id, "dances around")
```

### Commands API

```lua
-- Register a command
pvpgn.commands.register("/dance", function(session, args)
    pvpgn.chat.emote(session.account_id, "dances around")
    return true
end, {
    group = "users",
    description = "Dance emote"
})
```

### Events API

```lua
-- Subscribe to events
pvpgn.events.on("user_logged_in", function(event)
    print("User logged in: " .. event.name)
end)

pvpgn.events.on("channel_message_sent", function(event)
    print("Message in " .. event.channel_id .. ": " .. event.text)
end)
```

### Store API

```lua
-- Store is plugin-local key-value storage
pvpgn.store.put("key", "value")
local value = pvpgn.store.get("key")
```

### HTTP API

```lua
-- Make HTTP requests (requires net.http capability)
local response = pvpgn.http.get("https://api.example.com/data")
if response.status == 200 then
    print(response.body)
end
```

### Moderation API

```lua
-- Ban an account
pvpgn.moderation.ban_account(account_id, "Reason for ban", 86400) -- 1 day

-- Kick a connection
pvpgn.moderation.kick_connection(session_id)
```

## Native C++ Plugins

Native plugins implement the `IPlugin` interface:

```cpp
#include <infra/scripting/plugin/i_plugin.hpp>

class MyPlugin : public pvpgn::infra::scripting::IPlugin {
public:
    const PluginManifest& manifest() const override { ... }
    void init(PluginContext& context) override { ... }
    void shutdown() override { ... }
    CapabilitySet required_capabilities() const override { ... }
    bool is_ready() const override { ... }
};

extern "C" {
    pvpgn::infra::scripting::IPlugin* pvpgn_plugin_create() {
        return new MyPlugin();
    }
    
    void pvpgn_plugin_destroy(pvpgn::infra::scripting::IPlugin* plugin) {
        delete plugin;
    }
}
```

## Sandbox Restrictions

Plugins run in a sandbox with the following restrictions:

- No `os.execute()`, `os.remove()`, `os.rename()`
- No `io.open()` without `fs.read`/`fs.write` capability
- No `loadfile()`, `dofile()`, `require()` outside plugin directory
- No `debug` library (except `debug.traceback()`)
- Memory and CPU limits enforced per plugin

## Plugin Lifecycle

1. **Discovery** - Server scans `plugins/` directory
2. **Loading** - Plugin manifest is parsed and validated
3. **Initialization** - `init()` is called with `PluginContext`
4. **Running** - Plugin handles events and commands
5. **Shutdown** - `shutdown()` is called before unload
6. **Unloading** - Plugin is removed from memory

## Hot Reload

Plugins can be reloaded without restarting the server:

```
/admin reload-plugin quiz
```

Plugin state is preserved in `pvpgn.store`.

## Example Plugins

- `example-quiz/` - Quiz game with leaderboard
- `example-antihack/` - Anti-cheat detection
- `example-ghost/` - GHost++ bot integration

## Best Practices

1. **Declare all capabilities** - Only request what you need
2. **Handle errors gracefully** - Use try-catch in Lua
3. **Use store for persistence** - Don't write to disk directly
4. **Subscribe to events** - React to server events, don't poll
5. **Register commands** - Use the command API, not global functions
6. **Test in sandbox** - Verify your plugin works with restrictions

## Troubleshooting

### Plugin fails to load

Check the server logs for error messages. Common issues:

- Missing `plugin.toml`
- Invalid manifest syntax
- Missing required capabilities
- Lua syntax errors in entry point

### Plugin crashes

Plugins run in protected mode. Errors are caught and logged:

```
[ERROR] Plugin 'quiz' error: attempt to index nil value
```

### Performance issues

- Check for infinite loops in event handlers
- Use `pvpgn.store` instead of large Lua tables
- Consider native C++ plugin for performance-critical code

## Support

For questions or issues, see the main PvPGN documentation or open an issue on GitHub.
