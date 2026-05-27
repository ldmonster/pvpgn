# R194 Checklist -- WITH_BNETD=ON build triage

Started: 2026-05-27. Goal: with the systemic CMake ordering bug
repaired in R193.fix, attempt the WITH_BNETD=ON+PVPGN_BUILD_V3=ON
build and iteratively fix what surfaces.

## Progression

| Stage | Errors | Notes |
|-------|--------|-------|
| Initial | 49 | First attempt; mostly strangler_macros.h `extern "C"` at block scope |
| After fix 1 | 27 | Block-scope `extern "C"` removed (GCC15 rejected) |
| After fix 2/3/4 | 3 | Missing headers + missing struct include resolved |
| After fix 5 | 0 (bnetd_legacy clean) | Legacy prefs_load gated |
| Linking bnetd | 149 | New surface area: integration_legacy_bnetd_linked compile |
| After Findings 9+7 fixes | 7 | Down 95% just from prefs shim header + 4 dead `bnetd/prefs.h` includes |
| After Finding 8 + cleanup | 1 link error | All compile errors resolved |
| **After lib extract + init wiring** | **0** | **`bnetd` executable builds clean** |

## Fixes landed this round

1. **`src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/strangler_macros.h`**:
   moved the per-macro `extern "C" int pvpgn_v3_##name##_try(...)` forward
   declaration OUT of the `do { ... } while(0)` block (GCC 15+ rejects
   block-scope `extern "C"` as non-standard) and into a file-scope
   `extern "C" { ... }` block with all 9 try-functions explicitly listed.
   The macro now expands to just `do { if (...##_try(...) > 0) return 0; } while(0)`.

2. **`src/bnetd/server.cpp:70`**: replaced `#include "handle_init.h"`
   (header deleted by commit `03f35f9`) with a forward declaration of
   `pvpgn::bnetd::handle_init_packet`. The .cpp still exists and the
   v3-side override in `init_packet_dispatch_link.cpp` exists.

3. **`src/bnetd/irc.cpp:54`**: removed `#include "handle_wserv.h"`
   (header AND .cpp both deleted by commit `03f35f9`). At irc.cpp:2245
   replaced the `case conn_class_wserv: return handle_wserv_con_command(...)`
   call with `return irc_dispatch_con_command(...)` (fall-through to
   generic IRC dispatch) -- the wserv-specific handler is gone but the
   `conn_class_wserv` enum value is still referenced in 14 other sites
   so the class itself stays.

4. **`src/bnetd/handle_bnet.cpp:89`**: added
   `#include "integration/legacy_bnetd/send_friendslist_bridge.hpp"`
   under `#ifdef PVPGN_V3_BNETD_INTEGRATION`. Provides the
   `struct pvpgn_v3_friend_entry` definition that the
   `std::vector<pvpgn_v3_friend_entry>` at line 3326 was using.

5. **`src/bnetd/command.cpp:3968`**: renamed the local `Ctx::say`
   member to `Ctx::say_fn` to silence GCC 15+ `-Wchanges-meaning`
   (outer `say` lambda has the same identifier).

6. **`src/bnetd/server.cpp:1722-1726`**: gated the legacy
   `prefs_load()` reload-on-SIGHUP path under
   `#ifdef PVPGN_V3_BNETD_INTEGRATION` and routed the v3 branch
   through `pvpgn_v3_prefs_load_toml()` (computes the .toml sibling
   of the .conf preffile, same as `main.cpp:545-554`).

7. **`src/bnetd/server.cpp:1815`**: replaced
   `prefs_get_trackserv_addrs()` (legacy, deleted) with
   `prefs_v3::trackserv_addrs()` (same value, v3 shim).

## Verification

`docker run ... cmake --build /tmp/b --target bnetd_legacy` -> **0 errors**.

## Remaining: `bnetd` (executable) link/build

Building the full `bnetd` target pulls in `integration_legacy_bnetd_linked`
(the v3-side static lib). 149 errors surface, grouped:

### 7. Four v3 bridge .cpp files include deleted `bnetd/prefs.h`

- `src/v3/integration/legacy_bnetd/src/anongame_inforeply_bridge.cpp:41`
- `src/v3/integration/legacy_bnetd/src/get_icon_bridge.cpp:32`
- `src/v3/integration/legacy_bnetd/src/set_icon_bridge.cpp:36`
- `src/v3/integration/legacy_bnetd/src/tcp_bridge.cpp:29`

`bnetd/prefs.h` was deleted in R165. Replacement: either
`prefs_bridge.hpp` (the C-linkage v3 bridge) or `prefs_v3_shim.h`
(the inline C++ accessors). Need to grep what each .cpp actually
uses from prefs and pick the matching v3 replacement.

### 8. `LegacyAccountRepository` override mismatches

`src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/legacy_account_repository.hpp`
declares `find_by_id`, `find_by_name`, `remove`, `size` as
`override`, but the interface they claim to override has drifted
(probably renamed/return-type-changed methods). Need to inspect the
parent interface (`IAccountRepository`?) and reconcile.

Cascading: `install_v3_handlers.cpp:51,171` uses
`LegacyAccountRepository` as a value (not pointer), so when the
overrides are wrong the class is "abstract" and the field
declarations fail.

### 9. `prefs_v3_shim.h` shows ~80 `pvpgn_v3_prefs_get_X was not declared`

This is the same shape as R192 Finding 3 but at a DIFFERENT scope:
when v3 bridge .cpp files include legacy headers that pull in
`prefs_v3_shim.h`, those v3 TUs do NOT define `PVPGN_V3_BNETD_INTEGRATION`
(it's a bnetd_legacy-side gate). So shim.h's
`#ifdef PVPGN_V3_BNETD_INTEGRATION #include "prefs_bridge.hpp"` is
skipped, but the shim accessor function bodies unconditionally call
`pvpgn_v3_prefs_get_*` -> undeclared.

`bnetd_legacy` itself compiled fine because it DOES define the
macro. The shim is "header polluted" with macro-gated dependencies.

**Two fixes possible**:
- (a) Make `prefs_v3_shim.h` include `prefs_bridge.hpp`
  unconditionally (always-on). The bridge header itself has no
  PVPGN_V3_BNETD_INTEGRATION dependency.
- (b) Wrap each shim accessor body in the same macro guard so
  TUs that don't define it get no-op / stub accessors.

(a) is simpler; the only question is whether prefs_bridge.hpp is
universally safe to include in any TU.

## Status

- bnetd_legacy library builds clean under WITH_BNETD=ON+PVPGN_BUILD_V3=ON.
- bnetd executable: **all compile errors resolved**. 149 -> 0. One LINK error remains.
- v3-test docker (r194) regression check: green (174+ tests pass).
- 19 files changed, ~371 insertions / 275 deletions.

## Sweep fixes landed (Findings 7, 8, 9)

**Finding 9 -- `prefs_v3_shim.h` macro-gate (the big one)**:
`src/bnetd/prefs_v3_shim.h:26` -- replaced `#ifdef PVPGN_V3_BNETD_INTEGRATION #include prefs_bridge.hpp #endif` with `#if __has_include(...) ... #endif`. Now v3-side TUs that transitively pull in this shim see the bridge declarations regardless of the integration macro. **This single edit eliminated ~80 errors.**

**Finding 7 -- 4 v3 bridge files include deleted `bnetd/prefs.h`**:
- `anongame_inforeply_bridge.cpp`, `get_icon_bridge.cpp`, `set_icon_bridge.cpp`, `tcp_bridge.cpp`
- Replaced include with `integration/legacy_bnetd/prefs_bridge.hpp` (v3 C bridge)
- Replaced 5 `pvpgn::bnetd::prefs_get_*()` call sites with the `pvpgn_v3_prefs_get_*()` C bridge form (the legacy C++ wrappers were deleted in R165)

**Finding 8 -- `LegacyAccountRepository` interface drift**:
- Header: rewrote signatures to match current `IAccountRepository` (R166+): `find_by_id(uint32_t)`, `find_by_name(string_view)`, `save -> Result<void, Error>`, `remove(string_view)`. Added missing `exists/list_online/count` (latter two as `NotImplemented` stubs -- no caller uses them in the WITH_BNETD path).
- Removed legacy-only `size() noexcept` (replaced by `count()`).
- .cpp impl: rewired find_by_id (uint32_t direct), find_by_name (string_view direct), added stubs.

**Two trailing fixes**:
- `tcp_bridge.cpp:157` -- `conn_destroy(c, nullptr, FLAG)` was 3-arg; signature reduced to `conn_destroy(c, int)`. Fixed.
- `set_icon_bridge.cpp:65` -- last remaining `pvpgn::bnetd::prefs_get_anongame_infos_file()` -> `pvpgn_v3_prefs_get_anongame_infos_file()`.

## Remaining: one LINK error

```
server_v3_hook.cpp:(.text+0x4): undefined reference to `pvpgn::app::bnetd::LegacyBridge::instance()'
server_v3_hook.cpp:(.text+0xf): undefined reference to `pvpgn::app::bnetd::LegacyBridge::tick(...)'
```

Root cause: `legacy_bridge.cpp` (and its dep `asio_event_loop.cpp`) are compiled **only** into the standalone `pvpgn_v3_bnetd` executable, not into any library. The legacy `bnetd` exe (via `server_v3_hook.cpp` injected by R193 strangler wiring) tries to call `LegacyBridge::instance().tick()` but the symbols are unreachable from this binary.

Deeper concern: even if linked, no one calls `LegacyBridge::init(loop)` in the legacy `bnetd` startup path. The bridge would throw `std::logic_error` on first `server_tick_v3()` call -- the wiring was designed for the new exe.

### Three options for the link error

- **(A) Extract a small shared library**: pull `legacy_bridge.cpp` + `asio_event_loop.cpp` into a `app_bnetd_legacy_bridge` static lib, link both exes against it. Plus add `LegacyBridge::init()` call in legacy bnetd startup. Real work, restores design intent.
- **(B) Guard the call**: make `LegacyBridge::instance()` return `nullptr` when uninitialised and skip tick. `server_v3_hook.cpp` becomes safe to call uninit'd. Cleanest stopgap.
- **(C) Disable R193 server_v3_hook injection for the legacy bnetd path**: the integration block in `src/v3/integration/legacy_bnetd/CMakeLists.txt` that injects `server_v3_hook.cpp` into `bnetd_legacy` was added blindly -- if no v3 event loop runs in this exe, the hook should be a no-op stub.

## R194 round close

149 -> 0. **`bnetd` executable builds clean under WITH_BNETD=ON+PVPGN_BUILD_V3=ON.**

### Final fix: link error close (Option A)

Extracted `app_bnetd_legacy_bridge` static lib from
`src/v3/app/bnetd/CMakeLists.txt`:
- Sources: `asio_event_loop.cpp`, `legacy_bridge.cpp`
- Public include: `${CMAKE_CURRENT_SOURCE_DIR}/include`
- Public deps: `Boost::system`
- Public compile def: `PVPGN_V3_BNETD_INTEGRATION=1` (both TUs gate their bodies on it, so PUBLIC keeps consumers and the lib in ODR agreement).

Wired into both consumers:
- `pvpgn_v3_bnetd` exe: `target_link_libraries(... PRIVATE app_bnetd_legacy_bridge)`
- `bnetd` legacy exe: added through the R193 integration block (`if(TARGET app_bnetd_legacy_bridge) target_link_libraries(bnetd PRIVATE ...) endif()`).

Initialised the singleton in legacy main:
- `src/bnetd/main.cpp:105-111` -- added `#include "app/bnetd/asio_event_loop.hpp"` + `"app/bnetd/legacy_bridge.hpp"`
- `src/bnetd/main.cpp:594` (inside the existing `#ifdef PVPGN_V3_BNETD_INTEGRATION` block) -- added `pvpgn::app::bnetd::AsioEventLoop v3_legacy_bridge_loop;` + `LegacyBridge::init(v3_legacy_bridge_loop);`
- `src/bnetd/main.cpp:~655` (cleanup block) -- added `LegacyBridge::shutdown()` before the loop falls out of scope.

## Final status

- bnetd executable: **builds clean** under WITH_BNETD=ON+PVPGN_BUILD_V3=ON
- v3-test regression: all Catch2 suites pass (174+ tests)
- Files changed: 21 (~410 insertions / 280 deletions)
- Findings closed: 7, 8, 9 + link error
- Total errors: 149 -> 0
- Two remaining `-Wdelete-incomplete` warnings in `command.cpp:362,4355` are pre-existing and unrelated.
