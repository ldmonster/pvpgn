# R333 Checklist — Config docs + `legacy_prefs.hpp` retirement

## Goal
Generate a human-readable config reference document from the schema embedded
in `pvpgn-config --print-schema`, and assess whether `legacy_prefs.hpp` can
be retired.

## Steps

- [x] **Create `scripts/dev/gen-config-docs.sh`**
  - Runs `pvpgn_config_tool --print-schema` and wraps the output with a
    standard header/footer.
  - Writes the result to `docs/config-reference.md`.
  - Accepts `--tool <path>` and `--out <path>` overrides.
  - Made executable (`chmod +x`).

- [x] **Create `docs/config-reference.md`**
  - Static initial version (bootstrapped manually since the tool is not yet
    built in CI at doc-generation time).
  - Covers all 20 TOML sections with key / type / default / description.
  - Documents the `PVPGN_BNETD__<SECTION>__<KEY>` env-var override pattern.
  - Documents `env:<VAR>` and `file:<path>` indirection for Secret fields.
  - Marked `AUTO-GENERATED` with regeneration instructions.

- [x] **Assess `legacy_prefs.hpp` retirement**
  - **Decision: keep, do not delete.**
  - `legacy_prefs.hpp` is actively included by:
    - `src/v3/integration/legacy_bnetd/src/prefs_bridge.cpp`
    - `src/v3/infra/config/include/infra/config/prefs_dump.hpp`
    - `src/v3/integration/legacy_bnetd/src/d2dbs_prefs_bridge.cpp`
  - The shim bridges the legacy `prefs_get_*` API to the new `ServerConfig`
    struct.  Removing it would require rewriting all three consumers.
  - Updated the shim to use `.reveal()` on `Secret<std::string>` fields
    (`storage_dsn()` and `wol_autoupdate_password_str_` constructor init).
  - Added a `// TODO(R333): retire once all consumers are ported` comment
    to signal future intent.

## Notes
- `docs/config-reference.md` should be regenerated after any change to
  `ServerConfig` by running `scripts/dev/gen-config-docs.sh` with a built
  `pvpgn_config_tool` binary.
- The `--print-schema` output in `pvpgn-config/main.cpp` is the single
  source of truth for the schema; keep it in sync with `server_config.hpp`.
