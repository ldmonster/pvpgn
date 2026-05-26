# Phase 1 Step 10 — TOML prefs migration: wrap-up

Status: COMPLETE (R120 – R161)

## Problem

The legacy bnetd/d2cs/d2dbs configuration files (`bnetd.conf`,
`d2cs.conf`, `d2dbs.conf`) are an ad-hoc text format parsed by
hand-written code in `src/bnetd/prefs.cpp` (and equivalents). They
mix scalar prefs, file paths, lists, and free-form strings, all
through global mutable C-style `t_prefs`-like singletons. This
blocks v3 hexagonal layering -- there is no typed, immutable
ServerConfig the application layer can depend on -- and forces
tests to mock global file I/O.

## Approach

Strangler-fig migration toward TOML, in layered slices:

1. Introduce a TOML grammar (`toml++`-backed) and a typed
   `ServerConfig` in `infra::config`.
2. Mirror every legacy pref into a typed field on a sub-struct
   (`server`, `network`, `realm`, `log`, `files`, `misc`,
   `internal_`, `ladder`, ...).
3. Build a v3 parser per service (`parse_server_config`,
   `parse_d2cs_server_config`, `parse_d2dbs_server_config`) that
   yields `core::Result<ServerConfig, core::Error>`.
4. For each service, write an `extern "C"` *bridge* lib
   (`integration_legacy_<svc>`) that holds a process-global
   snapshot, exposes `_load_toml(path)` / `_unload()` /
   `_loaded()`, and a flat set of `_get_*` accessors mirroring
   the legacy `prefs_get_*` surface.
5. Gate legacy `prefs_get_*` call sites with
   `#ifdef PVPGN_V3_<SVC>_INTEGRATION` so the legacy build keeps
   working and the v3 build can opt in incrementally.
6. R159: add `D2csLegacyPrefs` / `D2dbsLegacyPrefs` adapters that
   own a `D2csServerConfig` / `D2dbsServerConfig` snapshot plus
   pre-computed `std::string` copies of every
   `filesystem::path` field, so bridge accessors can safely hand
   out `const char*` pointers with stable lifetime.
7. R161: refactor bridges to hold
   `std::shared_ptr<D2csLegacyPrefs>` / `<D2dbsLegacyPrefs>` --
   one source of truth, atomic hot-swap on reload, no separate
   `StringCache` in the bridge.

## Schema

Three typed configs:

- `ServerConfig`        (bnetd)  - 12 sub-sections,  ~180 fields
- `D2csServerConfig`    (d2cs)   -  7 sub-sections,  ~55  fields
- `D2dbsServerConfig`   (d2dbs)  -  5 sub-sections,  ~25  fields

All `path`-typed fields are `std::filesystem::path`. All list-typed
fields are `std::string` (parsed lazily by callers).

## Bridges

| Service | Library                       | Accessors |
|---------|-------------------------------|-----------|
| bnetd   | `integration_legacy_bnetd`    | ~150      |
| d2cs    | `integration_legacy_d2cs`    | ~55       |
| d2dbs   | `integration_legacy_d2dbs`   | ~25       |

Every bridge:
- holds exactly one snapshot via `shared_ptr<LegacyPrefs>` (R161),
- returns `const char*` from `std::string::data()` (null-terminated
  since C++11) backed by the adapter's owned strings,
- defaults to a freshly-constructed empty `LegacyPrefs` on
  parse failure (so accessors never crash; they return sentinel
  values).

## Tests

Catch2, all gated behind v3-test docker target.

| Suite                                     | Assertions | Cases |
|-------------------------------------------|-----------:|------:|
| `test_infra_config_server`                |       184  |   27  |
| `test_infra_config_d2cs_server`           |       105  |   13  |
| `test_infra_config_d2dbs_server`          |        54  |   10  |
| `test_infra_config_d2cs_legacy_prefs`     |        38  |    2  |
| `test_infra_config_d2dbs_legacy_prefs`    |        32  |    2  |
| `test_infra_config_conf_templates` (R161) |         9  |    3  |
| `test_infra_config_legacy_prefs`          |  (legacy)  |       |

Plus end-to-end docker `v3-test` stage that builds and runs every
`pvpgn_v3_add_test` target.

## R161 conf template audit

`conf/d2cs.toml.in` and `conf/d2dbs.toml.in` now ship every field
the v3 schema expects, including R160 additions
(`hide_pass_games`, `game_maxlevel`, `ladderlist_count`,
`difficulty_hack`). The new `conf_template_test.cpp` slurps each
`.in` file at test time (via `PVPGN_CONF_SOURCE_DIR`) and feeds it
through the v3 parser to guarantee they stay parseable as the
schema evolves.

## Non-goals (deferred to Step 11)

- Hot-reload signal handler that calls `_load_toml` then
  swaps the `shared_ptr` -- mechanism is in place, wiring is not.
- Removing the legacy `prefs_get_*` C functions -- still needed
  by every legacy translation unit that is not yet
  `PVPGN_V3_<SVC>_INTEGRATION`-gated.
- Editing/writing TOML from the admin telnet console.

## Round log

- R120-R150: schema, parsers, bnetd bridge + tests.
- R151-R152: d2cs/d2dbs bridge scaffolds.
- R157:      Wrap-up dialog.
- R158:      `D2csServerConfig` + `D2dbsServerConfig` parser tests
              (105 / 54 assertions).
- R159:      `D2csLegacyPrefs` + `D2dbsLegacyPrefs` adapters
              (38 / 32 assertions).
- R160:      Audit `conf/d2cs.toml.in` + `conf/d2dbs.toml.in`,
              add 4 missing keys.
- R161:      Wire adapters into bridges, add
              `conf_template_test.cpp`, write this note.
