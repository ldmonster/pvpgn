# Retiring `src/bnetd/prefs.cpp` / `prefs.h`

Round: **R147 (inventory + plan only — no code changes)**

## 1. Why this is still alive

After R146 every reasonable caller in `src/bnetd/` goes through
`prefs_v3_shim.h`. Yet `prefs.cpp` (3 377 lines, **82 `extern`
accessors**) still gets pulled into the bnetd binary because of three
distinct dependency classes:

| # | Class                                  | Concrete sites                                            | Blocks deletion? |
|---|----------------------------------------|-----------------------------------------------------------|------------------|
| A | Lifecycle entrypoints (`prefs_load` / `prefs_unload`) | `main.cpp:536`, `main.cpp:672`, `server.cpp:1714`, `server.cpp:1718` | **YES**          |
| B | One un-migrated direct accessor        | `server.cpp:1807` `prefs_get_trackserv_addrs()`           | YES (trivial)    |
| C | Shim "legacy fallback" branch          | **141 unique** `::pvpgn::bnetd::prefs_get_*()` calls in `prefs_v3_shim.h` | **YES**          |

Class C is the structural blocker: every shim accessor today reads

```cpp
if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_X();
return ::pvpgn::bnetd::prefs_get_X();
```

so as long as that second branch exists the linker will pull
`prefs.cpp` in even on a TOML-only deployment.

False positives (do **not** block deletion):

- `icons.cpp:561` / `icons.h:73` — `prefs_get_custom_icons()` is **defined
  inside `icons.cpp`**, not `prefs.cpp`. Misnamed, but lives elsewhere.
  The shim already routes `custom_icons()` straight to it. Out of scope.
- `handle_anongame.cpp:501,768` — comments only.
- All `prefs_get_*` self-references inside `prefs.cpp` itself.

## 2. Strategy: flip the polarity, then delete

The shim's "legacy fallback" was introduced as a safety net while TOML
loading was optional. To delete `prefs.cpp` we have to make TOML
loading non-optional, then erase the fallback in one mechanical pass.

### Phase A — Make `pvpgn_v3_prefs_load_toml()` always populate `g_prefs`

Today `pvpgn_v3_prefs_load_toml()` only sets `g_prefs` on parse success.
After this phase it must **always** leave a populated `g_prefs`:

- On parse success → populated from the file.
- On file-missing / parse-error → populated from `ServerConfig{}`
  (struct default-initialization, which already encodes every legacy
  hard-coded default).
- `pvpgn_v3_prefs_loaded()` then becomes a constant `true` once
  `main.cpp` reaches the call site.

Result: every shim accessor's `else` branch becomes statically
unreachable.

### Phase B — Migrate the four lifecycle call sites

| File:line              | Today                                              | After                                                                              |
|------------------------|----------------------------------------------------|------------------------------------------------------------------------------------|
| `main.cpp:536`         | `if (prefs_load(...) < 0) fatal-exit`              | `if (pvpgn_v3_prefs_load_toml(...) != 0) fatal-exit` (TOML missing = startup error) |
| `main.cpp:672`         | `prefs_unload();`                                  | `pvpgn_v3_prefs_unload();` (new no-op-or-clear bridge entry)                       |
| `server.cpp:1714,1718` | `prefs_load(...) < 0` inside `/reload`              | Same TOML reload that R146 already added, sole source                              |
| `server.cpp:1807`      | `tracker_set_servers(prefs_get_trackserv_addrs())`  | `tracker_set_servers(prefs_v3::trackaddrs())`                                       |

### Phase C — Strip every fallback branch in `prefs_v3_shim.h`

Mechanical: for every accessor of the form

```cpp
inline T name() {
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_name();
#endif
    return ::pvpgn::bnetd::prefs_get_name();
}
```

reduce to

```cpp
inline T name() { return pvpgn_v3_prefs_get_name(); }
```

and drop the `#include "prefs.h"` from the shim. 141 accessors. Pure
text edit, no semantic change once Phase A guarantees `g_prefs` is
always populated.

Special case: `custom_icons()` keeps calling
`::pvpgn::bnetd::prefs_get_custom_icons()` because that symbol lives
in `icons.cpp`, not `prefs.cpp`. Forward-declare it in the shim under
its own `namespace` block — `prefs.h` won't be pulled.

### Phase D — Delete

1. Drop `prefs.cpp` and `prefs.h` from `src/bnetd/CMakeLists.txt`.
2. Search the tree for `#include "prefs.h"` outside of `prefs.cpp`
   itself — every hit should compile after replacing with
   `#include "prefs_v3_shim.h"` (or already does).
3. `git rm src/bnetd/prefs.cpp src/bnetd/prefs.h`.
4. Rename `prefs_v3_shim.h` → `prefs_v3.h` (optional, cosmetic).
5. Docker `v3-build` + `v3-test` must stay green.

### Phase E — d2cs / d2dbs

`src/d2cs/prefs.cpp` and `src/d2dbs/prefs.cpp` are separate codebases
with their own accessors. They are **not** covered by R147. Once
`bnetd/prefs.cpp` is gone the same recipe applies to each (separate
ServerConfig already exists for `d2cs.toml` / `d2dbs.toml`).

## 3. Risk register

- **TOML parse regression**: Phase A makes a parse error fatal in
  Phase B. Mitigation: keep the "fall back to `ServerConfig{}`
  defaults" behaviour in Phase A; only Phase B's `main.cpp` change
  upgrades a missing file to a fatal — and even there the path is
  derived from the same cmdline arg, so an operator who has neither a
  `.conf` nor a `.toml` already cannot start the server.
- **Legacy plugin ABI**: `prefs_get_*` symbols are not exported from
  bnetd; no out-of-tree plugin should be calling them. Verified by
  reading `plugins/` — they use the public C SDK, not bnetd internals.
- **Lua / handle_anongame snapshot paths**: handled in R145 (lua
  config update) and R143 (`anongame_infos` reload). No further
  shim → bridge changes needed.

## 4. Execution rounds

| Round | Scope                                               | Expected file count |
|-------|-----------------------------------------------------|---------------------|
| R148  | Phase A + new `pvpgn_v3_prefs_unload()` bridge entry | 3 files (`prefs_bridge.{hpp,cpp}`, server_config defaults verified) |
| R149  | Phase B: 4 lifecycle migrations                     | 2 files (`main.cpp`, `server.cpp`)                                  |
| R150  | Phase C: mechanical shim flattening                 | 1 file  (`prefs_v3_shim.h`)                                          |
| R151  | Phase D: delete + CMake + Docker green              | ~5 files                                                             |
| R152+ | Phase E: replicate for d2cs / d2dbs                 | TBD                                                                  |

Each round ends with a docker `v3-build` + `v3-test` run, and the
round's checklist entry appended to `plans/step10-toml-checklist.md`.
