# 03 — Configuration: collapse to TOML

## What

Make `bnetd.toml` / `d2cs.toml` / `d2dbs.toml` the only configuration files installed by the v3 build. Move all data that lives in standalone `.conf` files **into TOML tables** unless that data is genuinely user-editable runtime content (channels, MOTD, news, bans).

## Why

Today `bnetd.toml` references 15 other `.conf` files (`channel.conf`, `topics.conf`, `bnban.conf`, `bnalias.conf`, `bnmaps.conf`, `bnxplevel.conf`, `bnxpcalc.conf`, `sql_DB_layout.conf`, `supportfile.conf`, `address_translation.conf`, `tournament.conf`, `command_groups.conf`, `anongame_infos.conf`, `icons.conf`, `realm.conf`). Each has its own bespoke parser. Most contain static configuration that never changes after install.

## Classification

| File | Nature | Target |
|------|--------|--------|
| `bnetd.toml.in` | Primary config | Keep, expand. |
| `d2cs.toml.in`, `d2dbs.toml.in` | Primary | Keep. |
| `versioncheck.json.in` | Large vendor data (1.9 kLOC) | **Keep as JSON**, but move to `share/pvpgn/versioncheck.json` (data, not config). Add reload via SIGHUP. |
| `bnban.conf.in` | Runtime mutable (IP bans) | **Keep as flat list** at `var/bnban.list`. Path configurable in `[files]`. |
| `channel.conf.in` | Runtime mutable | Keep, path in `[files]`. |
| `topics.conf.in` | Mostly static | Inline into `bnetd.toml` `[chat.topics]` array-of-tables. |
| `bnalias.conf.in` | Static | Inline `[chat.aliases]`. |
| `bnmaps.conf.in` | Static | Inline `[game.maps]`. |
| `bnxplevel.conf.in`, `bnxpcalc.conf.in` | Static math tables | Inline `[xp.levels]`, `[xp.calc]`. |
| `command_groups.conf.in` | Static | Inline `[admin.command_groups]`. |
| `sql_DB_layout.conf.in` | Schema | **Delete**: schema lives in `src/infra/{sqlite,mysql,postgres}/migrations/`. |
| `supportfile.conf.in` | Static UI text | Inline `[support]`. |
| `address_translation.conf.in` | Static | Inline `[network.nat]`. |
| `tournament.conf.in` | Static | Inline `[tournament]`. |
| `anongame_infos.conf.in` | Static | Inline `[anongame.infos]`. |
| `icons.conf.in` | Static | Inline `[icons]`. |
| `realm.conf.in` | Operator-edited list | Inline `[[realms]]` array-of-tables. |
| `ad.json.in` | Runtime mutable | Keep as JSON in `share/pvpgn/`. |
| `autoupdate.conf.in` | Vendor data (443 LOC) | Keep as separate, rename `autoupdate.toml`, schema = `[[file]]` AoT. |
| `bnetd_default_user.plain.in` | Default account template | Keep as `.plain`, lives in `share/pvpgn/`. |
| `bnissue.txt.in` | MOTD-style text | Keep as `.txt`. |

After this work, `conf/` contains:

```
bnetd.toml.in
d2cs.toml.in
d2dbs.toml.in
autoupdate.toml.in
i18n/
```

…and everything else moves to `share/` (data) or `var/` (mutable state).

## Concrete steps

1. **Schema first** — write `src/infra/config/server_config.hpp` extensions for each new section. One PR per section.
2. **Parser** — extend `infra/config/server_config.cpp` to read the new sections from TOML; add unit tests under `tests/unit/infra/config/`.
3. **Bridge** — update the relevant `prefs_bridge.cpp` getters to read from the new fields (and keep returning the legacy C-string shape so the legacy bnetd code keeps working until plan 06 retires it).
4. **Migration tool** — extend `pvpgn-migrate` with `pvpgn-migrate config --from-conf <dir> --to <bnetd.toml>` that reads the legacy files and emits the merged TOML.
5. **Install rules** — drop the obsolete `.conf.in` from `conf/CMakeLists.txt` install set.
6. **Docs** — extend `docs/toml-migration.md` with one section per inlined file.

## Acceptance criteria

- [ ] `conf/` contains only the 4 `.toml.in` files + `i18n/`.
- [ ] `bnetd-v3 --check-config` validates a config that used to need 15 `.conf` files.
- [ ] `pvpgn-migrate config` produces a byte-identical effective config (verified by a round-trip integration test).
- [ ] Legacy build path either consumes the same TOML or is gated behind `PVPGN_BUILD_LEGACY=ON` with a clear deprecation banner at start-up.

## Risks

- Operators have site-specific `channel.conf` / `realm.conf` content. **Mitigation**: the migrate tool is mandatory; document it; v3 binary refuses to start if it detects legacy `.conf` files in `${SYSCONFDIR}` and prints the exact `pvpgn-migrate` invocation.

## Out of scope

- TOML schema versioning. Bake `schema_version = 3` into the file; bump rules are in plan 11.
