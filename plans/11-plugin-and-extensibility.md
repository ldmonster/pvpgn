# 11 — Plugin and extensibility surface

## What

PvPGN-PRO already exposes:
- **Plugin C ABI 1.0** at `pvpgn/plugin/api.h` (`src/services/plugin/`).
- **Lua API v2** at `pvpgn.*` (`src/infra/scripting/lua/`).
- **TOML config schema** with `schema_version` field.

This plan defines how those three surfaces evolve so external contributors can ship features without forking pvpgn.

## Versioning rules

| Surface | Compat policy | Bump trigger |
|---------|--------------|--------------|
| Plugin C ABI | Append-only within major; struct layout fixed | Removing/reordering a member, changing a signature |
| Lua API v2  | Append-only within major; deprecation marker `pvpgn._deprecations` | Removing a function, changing argument order |
| TOML schema | Additive; renames go through a `[deprecated]` mirror table for one minor | Removing a key, changing semantics |

`docs/plugin-versioning-guide.md` is the canonical rulebook.

## Concrete steps

### Plugin C ABI

1. Add a conformance test target: `tests/integration/plugin_abi/`. Loads `plugins/example-quiz/` and asserts every public symbol resolves.
2. Pin the ABI version in `pvpgn/plugin/api.h` (`#define PVPGN_PLUGIN_ABI_VERSION 0x010000`) and in `plugin.toml` (`min_abi = "1.0.0"`).
3. Refuse to load plugins whose `min_abi` major doesn't match.
4. Ship a header-only "plugin SDK" via vcpkg port `pvpgn-plugin-sdk` for out-of-tree builds.

### Lua API v2

5. Every API function gets a Catch2 test in `tests/unit/infra/scripting/lua/api_test.cpp`.
6. `pvpgn.version()` returns `{major, minor, patch, abi}`.
7. Reflected schema: `pvpgn.config:dump()` returns the merged TOML as a Lua table for plugin authors to discover.
8. Sandboxing follows `docs/sandbox-integration-guide.md`; default-deny for `os.execute`, `io.popen`, raw sockets.

### TOML schema

9. `bnetd.toml` gains:

   ```toml
   schema_version = 3
   ```

10. Loader rejects unknown top-level keys when `schema_version >= 3` (typo guard).
11. `pvpgn-migrate config --upgrade` performs additive upgrades when a new minor bumps schema.

### Adding a new feature (the canonical path)

The docs page `docs/extending-pvpgn.md` (new, see plan 12) walks through one example end to end:

```
1. domain/<ctx>: add value object + port if needed.
2. application/<feature>: add command/handler.
3. infra/<tech>: add adapter implementing the port.
4. integration/<binding>: wire into the protocol that dispatches to it.
5. conf/bnetd.toml.in: add the TOML key(s) under a versioned section.
6. tests/unit/, tests/functional/, tests/integration/: tests at each layer.
7. CHANGELOG.md: "Added" entry.
8. docs/config-reference.md: regenerated from the schema.
```

## Acceptance criteria

- [ ] `plugins/example-quiz/` loads under `bnetd` without warnings.
- [ ] Removing a Lua API function in a PR fails the `lua_api_v2_conformance` test.
- [ ] Adding an unknown TOML key fails `bnetd --check-config`.
- [ ] `docs/extending-pvpgn.md` exists and is referenced from `README.md`.

## Risks

- Locking down `unknown_keys` breaks operators with hand-edited configs. Provide a one-release `[loader] strict = false` escape hatch.

## Out of scope

- A plugin marketplace.
- WASM plugins.
