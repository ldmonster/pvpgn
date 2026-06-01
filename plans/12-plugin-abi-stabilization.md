# 12 — Plugin ABI Stabilization

## What

Promote the plugin interface to a versioned, stable C ABI with a
capability model. Lua plugins, C++ plugins, and external scripts all
load through the same loader.

## Why

- Today plugins compile against C++ headers and break on every
  release. There is no semver contract.
- Capabilities (filesystem, network, persistence) are implicit — any
  plugin can do anything.
- Plan 11 needs a stable hook contract for tracing/metrics decoration.

## Prerequisites

- Wave-one plugin ABI conformance test exists. We extend it.

## Concrete steps

1. **`include/pvpgn/plugin/abi.h`** new public header. Pure C.
   - `pvpgn_plugin_v1_init(pvpgn_host_v1 const*, pvpgn_plugin_v1*)`
   - `pvpgn_plugin_v1_shutdown(pvpgn_plugin_v1*)`
   - Hook structs versioned independently
     (`pvpgn_chat_hooks_v1`, `pvpgn_auth_hooks_v1`, …).
2. **Capability tokens.** Plugin `.toml` manifest declares required
   capabilities (`net.outbound`, `fs.read:scripts/`, `db.read`,
   `db.write`, `lua.eval`). Host enforces at load time and at every
   call.
3. **Loader.** Move `infra/plugin/` to drive the C ABI. C++ and Lua
   plugin shims (`infra/plugin/cpp/`, `infra/plugin/lua/`) adapt the
   C ABI to those languages.
4. **Semver gate.** `scripts/dev/check-plugin-abi.sh` diffs the C
   header against the last tagged release; breaking changes require a
   `vN+1` header and a deprecation cycle for `vN`.
5. **Example plugins.** Migrate `plugins/example-quiz/`,
   `plugins/extra-commands/`, `plugins/ghost/`, `plugins/quiz/`,
   `plugins/antihack-starcraft/` to the new ABI. Delete those that
   can't be migrated and document why in `plugins/README.md`.
6. **Docs.** Update `docs/developer/extending-pvpgn.md` with: ABI
   versioning rules, capability list, end-to-end "hello plugin"
   walkthrough.

## Acceptance criteria

- [ ] Public C header `pvpgn/plugin/abi.h` exists, installed.
- [ ] All shipped plugins load via the new ABI; manifest declares
      capabilities; host enforces them.
- [ ] CI fails on breaking ABI change to `v1` without a new `v2`
      header and deprecation note.
- [ ] No C++ symbol from `domain/` or `application/` is exposed to
      plugins.
- [ ] `docs/developer/extending-pvpgn.md` updated with capability
      list.

## Risks

- Existing third-party plugins break. Mitigate with a `v0` legacy
  loader for one major release; deprecation announced in
  `CHANGELOG.md`.
- Capability enforcement at every call is overhead. Pre-resolve at
  load time; checks are O(1) bitmask tests.

## Out of scope

- Sandboxing untrusted plugins (process isolation is a wave-three
  concern).
- WebAssembly plugins.
