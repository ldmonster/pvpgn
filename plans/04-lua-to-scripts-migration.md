# 04 — Move `lua/` into `scripts/`

## What

Eliminate the top-level `lua/` directory. Move script content under `scripts/lua/`. Rewrite the loader so it lives next to the rest of operator-managed scripts and aligns with the Lua API v2 (`pvpgn.*`).

## Why

- Top-level `lua/` exists only because the legacy bnetd hard-coded a `${LOCALSTATEDIR}/lua` install location.
- `scripts/` already contains shell + perl helpers; consolidating eliminates a confusing parallel tree.
- The current `lua/` content uses the **v1** Lua API (`ah_init`, `gh_load`, `config.*`) which Lua API v2 supersedes (`pvpgn-api-v2.md`).
- Half of `lua/include/` (`bitwise.lua`, `convert.lua`, `math.lua`, `string.lua`, `table.lua`, `timer.lua`) is utility code that overlaps with stdlib Lua 5.3+ — YAGNI.

## Concrete steps

### Phase 4.1 — relocate

1. `git mv lua/ scripts/lua/`.
2. Update `lua/CMakeLists.txt` (now `scripts/lua/CMakeLists.txt`) to install to the same destination (`${LOCALSTATEDIR}/lua`) — no operator-visible change.
3. Update `bnetd.toml.in` default: `scriptdir = "${LOCALSTATEDIR}/lua"` (unchanged value, but document that it's now sourced from `scripts/lua/`).
4. Update `docs/lua-api-v2.md` paths.
5. Update `src/app/bnetd/src/main.cpp` comment block that references `lua/main.lua`.

### Phase 4.2 — modernize

6. Split `scripts/lua/` into:

   ```
   scripts/lua/
     boot/        # main.lua, config.lua, hook registration
     handlers/    # one file per event (was handle_*.lua)
     features/
       antihack/  # was lua/antihack/
       ghost/     # was lua/ghost/
       quiz/      # was lua/quiz/
       command/   # was lua/command/
       extend/    # was lua/extend/
     lib/         # was lua/include/, *minus* the redundant stdlib helpers
   ```

7. Delete `lua/include/{bitwise,convert,math,string,table,timer}.lua` and replace call sites with Lua 5.3+ builtins (`string.pack`, `<<`/`>>`, `os.time`).
8. Port `handle_*.lua` files to register via `pvpgn.hooks.register('on_channel_join', fn)` (v2 API) instead of relying on the legacy global function table.
9. Add a small `scripts/lua/boot/main.lua` that:
   - Reads `pvpgn.config` (v2-exposed TOML view).
   - Conditionally requires `features.*` based on TOML toggles (`[scripting.features]` table in `bnetd.toml`).

### Phase 4.3 — testing

10. Add a `tests/unit/infra/scripting/lua/` test that boots an embedded Lua VM with the v2 API, requires `scripts/lua/boot/main.lua`, and asserts the right hooks land on the bus.

## Acceptance criteria

- [ ] `lua/` at repo root no longer exists.
- [ ] `scripts/lua/boot/main.lua` runs unmodified against `bnetd-v3` without referencing any legacy global.
- [ ] All shipped `.lua` files pass `luacheck --std=lua54 --no-self`.
- [ ] No script imports `lua/include/string.lua` (deleted) or its siblings.

## Risks

- Site operators have local edits in `${LOCALSTATEDIR}/lua/`. Install behaviour does **not** overwrite existing files; document the migration in `docs/lua-api-v2.md`.
- The `pvpgn.hooks.register` v2 API may not yet cover every hook the v1 scripts used. Track each missing hook as a sub-task; do not delete the v1-style legacy entry points until coverage is 100%.

## Out of scope

- Swapping Lua for another scripting language.
- Sandboxing changes (covered by `docs/sandbox-integration-guide.md`).
