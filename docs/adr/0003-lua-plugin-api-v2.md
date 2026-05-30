# ADR 0003: Adopt Lua Plugin API v2 with `pvpgn.*` Namespace

**Date**: 2026-05-30  
**Status**: Accepted  
**Deciders**: PvPGN Core Team

## Context

PvPGN has supported Lua scripting since approximately R200.  The original
("v1") API exposed server internals as a flat collection of global functions
(`bnetd_send_chat`, `bnetd_get_account`, etc.) registered directly into the
Lua global table.

Problems with the v1 API:

- **Namespace pollution**: every server function occupied the global Lua
  namespace, making it easy for plugins to accidentally shadow built-ins or
  each other's globals.
- **No versioning**: there was no mechanism for a plugin to declare which API
  version it required, so a server upgrade could silently break plugins.
- **Tight coupling to legacy internals**: function names mirrored internal C
  struct names (`bnetd_account_t`, `d2cs_game_t`), leaking implementation
  details into the public API.
- **No event system**: plugins had to poll for state changes; there was no
  way to subscribe to server events (login, logout, game start, etc.).
- **No plugin isolation**: all scripts shared a single Lua state; a crash in
  one script could corrupt another.
- **Scripts lived in `lua/`** at the repo root, mixed with boot scripts and
  utility libraries, making it hard to package and distribute plugins.

The team evaluated:
- **Keeping v1 with incremental fixes**: rejected — the namespace and
  versioning problems are architectural and cannot be patched incrementally.
- **Switching to a different scripting language** (Python, JavaScript via
  QuickJS): rejected — Lua is already embedded, well-understood by the
  community, and has a tiny footprint.
- **Lua v2 with a namespaced API**: accepted.

## Decision

Introduce **Lua Plugin API v2**, characterized by:

1. **`pvpgn.*` namespace**: all server functions are registered under a
   single `pvpgn` table.  Sub-namespaces group related functionality:
   `pvpgn.commands`, `pvpgn.events`, `pvpgn.timer`, `pvpgn.config`.

2. **Explicit versioning**: every plugin declares `api_version_req` in its
   `plugin.toml` manifest.  The server rejects plugins whose requirement is
   not satisfied.

3. **Event-driven model**: `pvpgn.events.on(event_name, handler)` replaces
   polling.  The server emits typed events (`user_login`, `game_started`,
   etc.) and dispatches them to registered handlers.

4. **Command registration**: `pvpgn.commands.register(cmd, handler)` replaces
   the old pattern of hooking into the command dispatch table from Lua.

5. **Plugin directory structure**: each plugin is a directory under `plugins/`
   containing `plugin.toml` + `main.lua` (and optional helper modules).
   Feature scripts previously in `scripts/lua/` are migrated to `plugins/`
   (Plan 14).

6. **ABI conformance test**: `lua_api_v2_conformance` (in `tests/`) verifies
   that every function listed in the API contract is present and callable.
   Removing a function fails the test.

The v1 API is removed from v3 builds.  The legacy build
(`PVPGN_BUILD_LEGACY=ON`) retains v1 for backward compatibility until the
strangler migration is complete.

## Consequences

**Positive:**
- Plugins are self-contained, versioned, and distributable as directories.
- The `pvpgn.*` namespace prevents accidental global collisions.
- The event system enables reactive plugins without polling.
- The conformance test prevents accidental API breakage in PRs.
- `pvpgn.config.get()` gives plugins typed access to their own config
  section without parsing TOML manually.

**Negative / Trade-offs:**
- Existing v1 scripts must be rewritten to use the v2 API; no automatic
  migration path exists.
- The `pvpgn.*` table is a global injected by the C++ host; plugins that
  run outside PvPGN (e.g., in a standalone Lua interpreter for testing)
  must mock it.
- The `luacheck` linter must be configured with `pvpgn` as a known global
  (see `.luacheckrc` at the repo root) to avoid false-positive warnings.

**Related decisions:**
- ADR 0001 (TOML configuration) — `plugin.toml` uses the same TOML format.
- ADR 0002 (Hexagonal architecture) — the scripting subsystem lives in
  `src/infra/scripting/` and implements a port defined in `src/domain/`.
