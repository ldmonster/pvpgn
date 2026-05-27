# 14 — Legacy Retirement Roadmap

**Goal:** Delete `src/bnetd/`, `src/d2cs/`, `src/d2dbs/`,
`src/compat/`, `src/win32/`, the `bnetd_legacy`/`d2cs_legacy`/
`d2dbs_legacy` CMake targets, and the `PVPGN_BUILD_LEGACY` switch.

## 1. Why

Carrying the legacy tree forever:

- Doubles the build matrix and the audit surface.
- Tempts contributors to fix bugs in legacy instead of in v3.
- Pins the project to C++11 idioms (typedef, NULL, malloc) and to
  Unicode-on-Windows quirks (`setup_before.h`/`setup_after.h`).
- Blocks aggressive `-Werror` warning sweeps.

YAGNI: nobody needs both code paths once v3 is feature-complete.

## 2. Phases

### Phase A — Containment (R201–R232, in progress)

`02-finish-strangler-fig.md` moves every legacy `.cpp` into
`integration_legacy_*` and points all callers through v3 use cases.
After this, the `src/bnetd/` directory is mostly empty headers.

### Phase B — Adapter consolidation (R231–R260)

- `infra/persistence` replaces `storage_*.cpp`/`sql_*.cpp` (see
  `07-persistence-and-migrations.md`).
- `infra/lua` replaces legacy lua glue (see
  `13-plugin-and-scripting.md`).
- `infra/net/asio` replaces `connection.cpp` + `server.cpp` pump.
- `protocol/bnet|irc|telnet|wol` replace inline `handle_*` codecs
  (see `06-protocol-and-codecs.md`).

After Phase B, `integration_legacy_*` contains only thin shims that
delegate to v3 use cases.

### Phase C — Mandatory v3 (R261–R280)

- Make `PVPGN_V3_BNETD_INTEGRATION` mandatory: remove the
  `#ifdef` guards in `integration_legacy_*` and inline the
  strangler-fig call. The "fallback to legacy" path is deleted.
- Flip default of `PVPGN_BUILD_LEGACY` from ON to OFF in the top-level
  `CMakeLists.txt`. Release notes call this out as a major change.

### Phase D — One last release (R281–R290)

- Cut release `vN.0` with `PVPGN_BUILD_LEGACY=OFF` default. Source
  for legacy is still present, behind the option, so distros that
  haven't tested v3 yet can opt in.
- Document the migration in `docs/migration-from-legacy.md`.
- Wait one release cycle (~3 months) for operator feedback.

### Phase E — Removal (R291+)

- Delete:
  - `src/bnetd/`, `src/d2cs/`, `src/d2dbs/`,
  - `src/compat/` (most of its purpose was Windows/C89 portability;
    v3 is C++20 with std headers),
  - `src/win32/` (legacy Windows GUI shim — already disabled in v3),
  - top-level `Makefile` (legacy autotools-era convenience),
  - the `ConfigureChecks.cmake` (only used by legacy),
  - the `PVPGN_BUILD_LEGACY` option, all `if(PVPGN_BUILD_LEGACY)`
    branches in `CMakeLists.txt`.
- Move `src/v3/*` to `src/*` (i.e. drop the `v3/` prefix everywhere).
  This is purely cosmetic but signals the end of the migration.
- Bump major SemVer.

## 3. Backward-compat we keep forever

These survive Phase E:

- The on-wire BNet 1.x / IRC / Telnet / WoL protocol bytes.
- The `*.conf` data files for channels, MOTD, ipban, news, topics,
  helpfile, command_groups, address translation.
- TOML config schema (versioned, with migrations).
- Plugin C ABI (versioned, see `13-plugin-and-scripting.md`).

## 4. Backward-compat we drop in Phase E

- Building without C++20.
- Building under GCC < 12, MSVC < 19.38, Clang < 15.
- Configure-time options that only mattered for legacy targets.
- The legacy plaintext account format (replaced by TOML-per-account
  in `07-persistence-and-migrations.md`; converter ships in Phase D).
- The `bnetd.conf` (non-TOML) format — `bnetd.conf` → `bnetd.toml`
  was complete pre-v3; the .conf reader is deleted with prefs.cpp.

## 5. Risk register

| Risk | Mitigation |
|------|------------|
| Distros packaging older clients ship pvpgn with legacy enabled | Phase D gives one full release of overlap. |
| Plaintext-account operators don't migrate | Block startup with a clear "run `pvpgn-migrate --plain-to-toml`" message. Don't auto-convert. |
| Plugin authors not done with Lua API v2 | Phase D ships both Lua v1 and v2; deprecation warnings on v1. |
| Forgotten Windows GUI users | Confirm via release announcement; the v3 Windows builds run as a console daemon. |

## 6. Concrete tasks

- [ ] R303 (start of Phase C): flip `PVPGN_V3_BNETD_INTEGRATION` to
      mandatory.
- [ ] R304: change default of `PVPGN_BUILD_LEGACY` to OFF.
- [ ] R305: cut release `vN.0`.
- [ ] R306 (Phase E, after the soak window): bulk delete legacy
      sources, drop the option, move `src/v3/*` → `src/*`.
