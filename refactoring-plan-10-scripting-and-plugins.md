# 10 · Scripting (Lua) & Plug-in System

The legacy Lua integration (`bnetd/luainterface*.cpp`,
`luawrapper.cpp`, `luaobjects.cpp`, `luafunctions.cpp`) is a hand-rolled
C bridge that marshals raw `t_account`/`t_connection`/`t_channel`/`t_game`
pointers as Lua userdata. It is brittle (any struct change breaks
scripts), insecure (full `os` stdlib), single-threaded, and impossible
to test.

## 1. Goals

* **Stable scripting API** that survives internal refactors (talk to
  application-layer DTOs, not domain pointers).
* **Sandboxed**: scripts cannot exec shell, read arbitrary files, or
  open sockets unless granted a capability.
* **Fiber-aware**: a script can do `db.find_account("alice")` and the
  underlying repository call yields the fiber, not blocks the thread.
* **Multi-runtime ready**: a Lua plug-in and a future native C++
  plug-in implement the same `IPlugin` interface.
* **Extension marketplace**: package format that ships
  scripts/resources/assets together.

## 2. Engine choice

Use **sol3** (modern C++ Lua binding) targeting **Lua 5.4** (with a
build-time fallback to LuaJIT 2.1 for x86_64 perf-sensitive
deployments). Sol3 supports:

* Automatic binding of typed C++ functions and structs.
* `protected_function` calls (no scripts crashing the host).
* Custom userdata with full `usertype` definitions.
* Coroutine integration we hook into Boost.Fiber.

## 3. New layout

```
src/infrastructure/scripting/
├── plugin/
│   ├── i_plugin.hpp              # contract for all plug-ins
│   ├── plugin_loader.{hpp,cpp}   # discovers & loads
│   ├── plugin_manifest.hpp       # plugin.toml schema
│   └── capability.hpp            # network, fs, db, exec, …
├── lua/
│   ├── lua_host.{hpp,cpp}        # owns sol::state per plug-in
│   ├── sandbox.{hpp,cpp}         # restricts os/io/debug
│   ├── bindings/
│   │   ├── account_view.{hpp,cpp}
│   │   ├── channel_view.{hpp,cpp}
│   │   ├── game_view.{hpp,cpp}
│   │   ├── command_api.{hpp,cpp}
│   │   ├── event_api.{hpp,cpp}
│   │   ├── chat_api.{hpp,cpp}
│   │   ├── moderation_api.{hpp,cpp}
│   │   └── http_api.{hpp,cpp}    # outbound HTTP via Beast (opt-in)
│   └── fiber_glue.{hpp,cpp}      # makes sol3 yield to fibers
└── builtin/
    └── (no built-in plug-ins; defaults shipped in lua/)
```

## 4. Plug-in contract

Every plug-in directory contains a `plugin.toml`:

```toml
name        = "tournament-pack"
version     = "1.2.0"
authors     = ["alice@example.com"]
api         = "3.0"
entry       = "main.lua"        # or "lib.so" for native
capabilities = ["chat.send", "db.read", "events.subscribe"]
description  = "Adds /tournament management commands."
```

Loaded plug-ins must:

* Register handlers via the typed API (no monkey-patching globals).
* Declare capabilities; missing capability → API call throws.
* Run only on permitted fibers (one per plug-in by default).

## 5. Lua scripting API (3.0)

Stable contract; future internal refactors must keep these signatures
or bump `api`:

```lua
-- Identity
pvpgn.account.find_by_name(name)        -- → AccountView | nil
pvpgn.account.list({page=1, pageSize=50})

-- Chat
pvpgn.chat.send_channel(channel_id, text)
pvpgn.chat.send_whisper(from_id, to_id, text)

-- Commands
pvpgn.commands.register("/dance", function(session, args)
    pvpgn.chat.emote(session.account_id, "dances around"); return true
end, {group="users"})

-- Events
pvpgn.events.on("user_logged_in", function(e)
    pvpgn.chat.send_whisper(e.account_id, e.account_id,
        "Welcome back, " .. e.name .. "!")
end)

-- Persistence (KV per plug-in, isolated namespace)
pvpgn.store.put("greeted." .. e.account_id, true)
pvpgn.store.get("greeted." .. e.account_id)

-- HTTP (capability required)
local r = pvpgn.http.get("https://api.example.com/news")
```

Underneath, each binding calls a `use-case` through `ICommandBus`. The
script's only knowledge is the public DTO and the stable event names.

## 6. Compat shim for legacy scripts

The existing `lua/handle_*.lua` and `lua/main.lua` use `t_account`-like
userdata. A compatibility module loads first and exposes:

```lua
function handle_command(account_table, command, args) ... end
```

…internally translating to the new API. The shim is marked
**deprecated** and a config option `scripting.legacy = false` disables
it in 4.0.

## 7. Sandbox

* No `os.execute`, `os.remove`, `os.rename`, `os.tmpname`.
* No `io.open` without `fs.write` capability.
* No `loadfile`/`dofile`/`require` outside the plug-in directory.
* `debug` library removed except `debug.traceback`.
* CPU/memory caps via Lua `lua_setallocf` and instruction-count hooks.
* Per-plug-in fiber priority and stack quota.

## 8. Native (C++) plug-ins

Same `IPlugin` interface, loaded from `.so`/`.dll`:

```cpp
class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual const Manifest& manifest() const = 0;
    virtual void init(PluginContext&)        = 0;
    virtual void shutdown()                  = 0;
};
extern "C" pvpgn::IPlugin* pvpgn_plugin_create();
```

`PluginContext` exposes the same DI container as the composition root
but **filtered by capabilities**.

## 9. Hot reload

* `SIGHUP` or a web-UI button triggers `PluginLoader::reload(name)`.
* Each plug-in is stopped (callbacks unregistered) and reloaded.
* `pvpgn.store` data persists across reload.

## 10. Plug-in distribution

* A tarball/zip with `plugin.toml` at the root.
* Loaded from a configurable `plugins/` directory.
* Future: `pvpgn plugin install <name>` pulls from a Git or HTTP
  registry (out-of-scope for 3.0).
