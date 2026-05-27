# 13 — Plugin ABI & Scripting

**Goal:** Two officially-supported extension surfaces — **native
plugins** (C ABI, dynamically loaded) and **Lua scripts**
(in-process VM). Both are versioned, sandboxed, and decoupled from
internal C++ headers.

## 1. Native plugin ABI

### Current state
`plugins/README.md` describes a legacy plugin system; only
`example-quiz/` exists. Plugins today are loaded by the legacy
bnetd directly and have access to far too much internal state.

### Target

- Plugin entry points are **C** symbols, not C++.
- Stable header `include/pvpgn/plugin/api.h` (lives in
  `src/v3/integration/plugin_host/include/`).
- ABI is **opaque pointer + function table**:

```c
typedef struct pvpgn_plugin_host pvpgn_plugin_host_t;
typedef struct pvpgn_plugin      pvpgn_plugin_t;

#define PVPGN_PLUGIN_API_VERSION_MAJOR 1
#define PVPGN_PLUGIN_API_VERSION_MINOR 0

typedef struct {
    uint32_t    abi_major;   // must equal API_VERSION_MAJOR
    uint32_t    abi_minor;   // <= API_VERSION_MINOR
    const char* name;
    const char* version;
    pvpgn_status_t (*on_load)(pvpgn_plugin_t*, pvpgn_plugin_host_t*);
    pvpgn_status_t (*on_unload)(pvpgn_plugin_t*);
    /* event hook table — opted into in on_load */
} pvpgn_plugin_descriptor_t;

PVPGN_PLUGIN_EXPORT const pvpgn_plugin_descriptor_t*
    pvpgn_plugin_entry(void);
```

- Host services exposed via `pvpgn_plugin_host_t` are a **vtable** of
  C function pointers (broadcast message, register command, query
  account by name, etc.). No raw access to C++ objects.
- Plugins live in their own process directory; the host scans
  `plugins/*.so|*.dll` on startup and after a privileged admin
  `/admin/plugins/reload` call.
- Plugin failures (load, hook exception in adapter) are isolated:
  the offending plugin is unloaded, the host stays up.

### SemVer

- Adding a new function to the host vtable = minor bump (existing
  plugins keep working because `abi_minor` check is `<=`).
- Removing or changing the signature of a function = major bump
  (rare; plan to never do this until v2).
- Hook signatures changing = major bump.

### Sandbox

- Plugin code runs in the host process for now (out-of-process
  plugins are future work). Mitigations:
  - The vtable is the **only** API surface; plugins must not link
    pvpgn headers.
  - Plugins can be denied filesystem/network at load time via
    seccomp on Linux (`infra/sandbox/seccomp`) — existing
    `infra/sandbox/` folder is the right home.
  - Resource caps optional but tracked.

## 2. Lua scripting

### Current state

- `lua/` directory holds the existing Lua glue
  (`handle_*.lua`, `extend/`, `command/`, `quiz/`, `antihack/`,
  `ghost/`, etc.).
- The C++ side is in `src/bnetd/lua*.cpp` (legacy) and is on the
  R226 relocation list.

### Target

- Replace hand-rolled bindings with **sol2 v3** in
  `infra/lua/sol/`. sol2 is header-only, ergonomic, and battle-tested.
- A `ScriptHost` port in `application/scripting/ports/ScriptHost.h`
  with: `load_dir(path)`, `call_hook(name, args)`, `register_command(
  name, lua_callback)`. Adapter wraps sol2.
- The Lua surface is **the documented one**, not "whatever C++
  symbols we happen to expose". Surface =
  - account read-only views,
  - chat broadcast helpers,
  - command registration,
  - event hooks (`on_login`, `on_channel_join`, `on_message`,
    `on_game_start`, `on_game_end`).
- All Lua-callable functions return tables, not raw pointers.
- Per-script sandbox via sol's environment table: scripts get a
  whitelisted global env (`string`, `math`, `table`, `os.time`,
  `os.date`, custom `pvpgn` table). `io`, `os.execute`, `package`,
  `require` of arbitrary paths are removed.
- Instruction count limit via `lua_sethook(L, hook, LUA_MASKCOUNT,
  N)` — abort runaway scripts.
- Memory limit via custom allocator.

### Migration path

1. R226: relocate legacy lua glue into `integration_legacy_bnetd`.
2. R296: introduce sol2 + `ScriptHost` port in parallel; expose the
   v3 API behind a feature flag (`lua.api_version = 2`).
3. R297–R299: port existing `lua/*.lua` scripts to v2 API; ship a
   compatibility shim layer that wraps v1 names where it can.
4. R300: drop v1 Lua API and remove legacy lua glue.

## 3. Versioning & documentation

- `docs/plugin-versioning-guide.md` already exists — keep and
  update.
- `docs/plugin-api.md` (new) — full vtable + hook reference.
- `docs/lua-api.md` (new) — generated from a small Lua doc comment
  scraper in `scripts/dev/`.

## 4. Concrete tasks

- [ ] R298: stabilise `pvpgn/plugin/api.h` (1.0), example-quiz
      ported to it.
- [ ] R299: seccomp sandbox for Linux plugins.
- [ ] R300: sol2 adapter + `ScriptHost`.
- [ ] R301: rewrite lua bindings around new port; deprecate
      legacy lua glue.
- [ ] R302: plugin/lua docs generators.
