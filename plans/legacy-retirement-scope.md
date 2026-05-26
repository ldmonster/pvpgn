# Legacy build retirement — scoping note

> Written R166. Scopes the work to retire `PVPGN_BUILD_LEGACY`.
> No code changes in R166; this is a roadmap.

## Current shape (post-R165)

- Root option `PVPGN_BUILD_LEGACY` defaults ON.
- `bnetd_legacy`, `d2cs_legacy`, `d2dbs_legacy` static libraries
  hold ~95% of the legacy code paths. The legacy `bnetd` /
  `d2cs` / `d2dbs` executables link these directly.
- `pvpgn_v3_bnetd` (v3 daemon) does NOT link `bnetd_legacy` --
  it is a fully standalone Boost.Asio service.
- Conversely, `bnetd_legacy` links `integration_legacy_bnetd_linked`
  (and v3 infra libs by transit) for the prefs bridge and the
  R142+ strangler-fig hooks.

## Why we cannot just flip it OFF today

1. `bnetd` (legacy binary) is still the production workhorse. The
   v3 daemon implements only ~20% of the protocol (session +
   auth + irc fsm + a few packet wires). Disabling legacy =
   shipping a non-functional server.
2. Several integration libs (`integration_legacy_*_linked`) pull
   in legacy headers via `setup_before.h` / `setup_after.h`. They
   cannot compile without the legacy tree on the include path.
3. CMakeLists in `src/bnetd/`, `src/d2cs/`, `src/d2dbs/`,
   `src/bntrackd/`, `src/bnpass/`, `src/tools/*` ALL depend on
   `PVPGN_BUILD_LEGACY`. A clean removal is ~30 CMakeLists edits.

## Suggested staged retirement

| Stage | Action                                                          | Gate                                         |
| ----- | --------------------------------------------------------------- | -------------------------------------------- |
| L0    | Make `pvpgn_v3_bnetd` feature-complete for chat + game lobby    | Phase 3.A-D done                             |
| L1    | Cut over Dockerfile.v3 to run `pvpgn_v3_bnetd` instead of bnetd | smoke tests green                            |
| L2    | Replace `bnetd` binary build with a thin shim that exec's v3   | release notes published                     |
| L3    | Delete `src/bnetd/{main,winmain}.cpp`, retain bnetd_legacy lib  | one release with shim in production         |
| L4    | Migrate remaining bnetd_legacy TUs onto v3 (Phase 3 finish)     | bnetd_legacy is empty                       |
| L5    | Remove `PVPGN_BUILD_LEGACY` option; same for d2cs/d2dbs         | all legacy targets unused                   |

Each L-step is roughly one round of work (some larger).

## What we WILL do this round (R166)

Nothing in code -- only this doc + Phase 3 plan. Retirement is not
safe until Phase 3.A-D land.
