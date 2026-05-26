# Audit: deletion of legacy `prefs_get_*` C shims

Round R162. Goal: determine whether the legacy `prefs.cpp` /
`prefs.h` translation units in `src/bnetd/`, `src/d2cs/`, and
`src/d2dbs/` can be deleted now that the TOML migration is
functionally complete.

## Method

Two sweeps:

1. All `*.cpp` / `*.c` files under `src/{bnetd,d2cs,d2dbs}/`
   excluding `prefs*.cpp` / `prefs_v3_shim.h`, grepped for any
   `prefs_get_<name>(` call site that is not a re-declaration
   (`extern char const * ...`).
2. Same sweep across `src/lua/` and `src/plugins/`.

## Results

| Tree                          | Direct callers found |
|-------------------------------|---------------------:|
| `src/bnetd/*.cpp`             |                    1 |
| `src/d2cs/*.cpp`              |                    0 |
| `src/d2dbs/*.cpp`             |                    0 |
| `src/{bnetd,d2cs,d2dbs}/*.h`  |                    0 |
| `src/lua/`                    |                    0 |
| `src/plugins/`                |                    0 |

The single straggler in `src/bnetd/server.cpp:1807`
(`tracker_set_servers(prefs_get_trackserv_addrs())`) was fixed in
this round -- routed through `pvpgn::bnetd::prefs_v3::trackserv_addrs()`
which already exists in the shim.

## Conclusion

After the R162 fix, **every direct `prefs_get_*` call site in the
project goes through the `prefs_v3` shim**. The shim dispatches
to the v3 bridge under `PVPGN_V3_<SVC>_INTEGRATION` and to the
legacy `prefs_get_*` C function in the `#else` branch (non-v3
build).

The `prefs.cpp` / `prefs.h` files themselves cannot be deleted
yet, because:

- They are still compiled into the legacy build (`-DPVPGN_V3=OFF`),
  which is the active production target for downstream packagers
  while v3 stabilizes. Their `extern char const * prefs_get_*`
  function definitions are what the shim's `#else` branch resolves
  to.
- Under the v3 build, the per-service `CMakeLists.txt`
  (`D2CS_LIB_SOURCES`, `D2DBS_LIB_SOURCES`,
  `bnetd_legacy` sources) already drop `prefs.cpp` (R151 / R156 /
  R157), so the file is dead code at link time. Only the *legacy*
  build link-pulls it.

## Path to actual deletion

Two milestones gate deletion:

1. **Drop the non-v3 build path** from `src/{bnetd,d2cs,d2dbs}/`
   (i.e., remove the `#else` branch from every `prefs_v3_shim.h`
   accessor and flatten the shim to a thin v3-only namespace).
   This is Phase 1 Step 11 closeout work.
2. **Delete `prefs.cpp` + `prefs.h`** from the three service
   trees, along with the `t_prefs` struct, the
   `prefs_load`/`prefs_unload`/`prefs_reload` entry points, the
   conf-file lexer/tokenizer, and the per-service `*.conf.in`
   templates (kept for now as legacy-readable documentation).

Both steps are mechanical once the non-v3 build is retired.

## Recommendation

Defer deletion to Phase 1 Step 11. No further "audit" work is
needed in Step 10 -- the shim is the single chokepoint and is
fully populated.
