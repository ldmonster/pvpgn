# Scripts & Tooling

This page catalogs the helper scripts shipped under `scripts/`. It exists so
every shipped script is discoverable and accounted for — the local gate
`scripts/dev/check-scripts-orphans.sh` fails if any script under `scripts/` is
referenced nowhere, which keeps dead one-shot scaffolding from accumulating.

> One-shot migration scripts from completed refactoring rounds are deleted once
> their job is done — they are not kept "just in case" (YAGNI). What remains
> below is the set of scripts with an ongoing purpose.

## Developer quality gates (`scripts/dev/`)

The single entry point is `scripts/dev/check-all.sh`, which runs the
architectural and fast-test gates (see
[Local quality gates](../../plans/11-local-quality-gates.md)). It orchestrates:

- `scripts/v3_layering_check.sh` — enforces the layer dependency rule.
- `scripts/check_domain_purity.sh` — forbids I/O, clocks, logging, and global
  state in the domain layer.
- `scripts/dev/check-unit-pairing.sh` — every domain/application TU has a test.
- `scripts/dev/check-test-legacy-linkage.sh` — tests don't link legacy targets.
- `scripts/dev/check-plugin-abi.sh` / `scripts/dev/check-plugin-abi-purity.sh`
  — the plugin C ABI is semver-stable and C99-pure.
- `scripts/dev/check-changelog.sh` — `CHANGELOG.md` follows Keep a Changelog.
- `scripts/dev/check-config-reference-sync.sh` — config docs match the schema.
- `scripts/dev/check-docs-reachable.sh` — every nav page is reachable.
- `scripts/dev/check-scripts-orphans.sh` — this catalog stays complete.
- `scripts/dev/check-coverage.sh` — coverage floor (deep gate).

Documentation generators (run after a build):

- `scripts/dev/gen-config-docs.sh` — regenerates `config-reference.md`.
- `scripts/dev/gen-lua-docs.sh` — regenerates the Lua API reference.
- `scripts/dev/gen-plugin-docs.sh` — regenerates the plugin API reference.

Performance & quality:

- `scripts/dev/run-bench.sh` + `scripts/dev/check-bench-regression.py` — the
  microbench harness and its regression gate.
- `scripts/dev/mutation_pilot.py` — mutation-testing pilot.

## Lua scripting API (`scripts/lua/`)

These files are the shipped Lua scripting surface loaded by the server's Lua
runtime (see [Lua API v2](lua-api-v2.md)). They are the live default scripts,
not examples.

- Entry point & config: `scripts/lua/main.lua`, `scripts/lua/config.lua`.
- Event handlers: `scripts/lua/handle_channel.lua`,
  `scripts/lua/handle_client.lua`, `scripts/lua/handle_command.lua`,
  `scripts/lua/handle_game.lua`, `scripts/lua/handle_server.lua`,
  `scripts/lua/handle_user.lua`.
- API extensions: `scripts/lua/extend/account.lua`,
  `scripts/lua/extend/account_wrap.lua`, `scripts/lua/extend/channel.lua`,
  `scripts/lua/extend/eventlog.lua`, `scripts/lua/extend/game.lua`,
  `scripts/lua/extend/message.lua`.
- Enum tables: `scripts/lua/extend/enum/attr.lua`,
  `scripts/lua/extend/enum/eventlog.lua`, `scripts/lua/extend/enum/game.lua`,
  `scripts/lua/extend/enum/message.lua`,
  `scripts/lua/extend/enum/messagebox.lua`, `scripts/lua/extend/enum/tag.lua`.
- Stdlib helpers: `scripts/lua/include/bitwise.lua`,
  `scripts/lua/include/common.lua`, `scripts/lua/include/convert.lua`,
  `scripts/lua/include/file.lua`, `scripts/lua/include/math.lua`,
  `scripts/lua/include/string.lua`, `scripts/lua/include/table.lua`,
  `scripts/lua/include/timer.lua`.

## Operator & deployment (`scripts/`)

- `scripts/bnetd.init` — SysV init script for the bnetd daemon.
- `scripts/bnetd.service` — systemd unit for the bnetd daemon.
- `scripts/bnetd.logrotate` — logrotate fragment for bnetd logs.
- `scripts/announce.sh` — broadcast an announcement to a running server.
- `scripts/bnetmasq.sh` — masquerade/NAT helper for LAN game discovery.
- `scripts/docker-entrypoint.sh` — container entrypoint.

## Maintenance utilities

- `scripts/ladder.py` — recompute/export ladder standings.
- `scripts/sql/repairlevels.pl` — repair character level data in SQL storage.
- `scripts/storage/cdb2sql.pl` — convert legacy CDB account storage to SQL.
- `scripts/storage/plain2sql.pl` — convert plaintext account storage to SQL.

## Localization tooling (`scripts/localize/`)

- `scripts/localize/pvpgn_localize_generator.cs` — generate localization
  tables.
- `scripts/localize/pvpgn_localize_validator.cs` — validate localization files.
- `scripts/localize/update.bat` / `scripts/localize/validate.bat` — Windows
  wrappers for the two C# tools.
