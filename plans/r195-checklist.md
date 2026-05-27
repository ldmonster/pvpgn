# R195 -- CI regression lane: `v3-bnetd-on` Dockerfile stage

## Goal

Add a CI lane that exercises the legacy `bnetd` executable build
with `WITH_BNETD=ON + PVPGN_BUILD_V3=ON` so the dead-code period
that ran R168..R191.a never recurs silently. R194 made the build
work; R195 keeps it working.

## Implementation

`Dockerfile.v3` gains a new stage `v3-bnetd-on`:

- Inherits from `v3-base` (Alpine + boost + openssl + zlib + curl).
- Configures with `-D PVPGN_BUILD_V3=ON -D PVPGN_BUILD_LEGACY=ON
  -D WITH_BNETD=ON -D WITH_D2CS=OFF -D WITH_D2DBS=OFF
  -D PVPGN_V3_WARNINGS_AS_ERRORS=OFF -D CMAKE_BUILD_TYPE=Release`.
- Builds the `bnetd` target only (not d2cs/d2dbs/tools -- the goal
  is the strangler-fig integration gate, not full distro packaging).
- Verifies the produced binary exists at
  `build/v3-bnetd-on/src/bnetd/bnetd`.
- `CMD` runs `bnetd --help` to confirm startup is not catastrophic.

Stage placement: appended at the very bottom of `Dockerfile.v3`
after `v3-runtime`. Independent of the existing v3-test pipeline
so it can be built / run separately:

```bash
docker build -f Dockerfile.v3 --target v3-bnetd-on -t pvpgn:v3-bnetd-on .
```

## Why `PVPGN_V3_WARNINGS_AS_ERRORS=OFF`

The legacy bnetd headers do not warning-clean under the v3 strict
warning set (-Wshadow / -Wold-style-cast / -Wcast-align /
-Woverloaded-virtual / -Wnull-dereference / -Wdouble-promotion /
-Wformat=2). The strangler-fig integration target
(`integration_legacy_bnetd_linked`) already does
`target_compile_options(... -w)` in `src/v3/integration/legacy_bnetd/CMakeLists.txt`
for this reason; the legacy `bnetd_legacy` lib carries the same
warning blanket. Forcing `-Werror` would defeat the build before
it could test the actual integration. Restoring strict warnings on
the legacy tree is out of scope for R195 and probably forever
(the C++17 idioms are too embedded).

## Why only the `bnetd` target

Other legacy daemons (`d2cs`, `d2dbs`) have their own integration
surfaces (`integration_legacy_d2cs`, `integration_legacy_d2dbs`)
that were not exercised by R194. Adding them to the CI lane would
risk surfacing additional latent breakage that this round didn't
plan to fix. Each can be added in a follow-up round once the
respective `with_*_ON` build is similarly verified.

## Verification

```
docker build -f Dockerfile.v3 --target v3-bnetd-on -t pvpgn-v3-bnetd-on:r195 .
...
#9 62.47 [ 58%] Built target bnetd_legacy
#9 91.26 [100%] Built target bnetd
#9 91.26 v3-bnetd-on: built bnetd successfully at build/v3-bnetd-on/src/bnetd/bnetd
#10 naming to docker.io/library/pvpgn-v3-bnetd-on:r195 done
```

Two pre-existing `-Wdelete-incomplete` warnings in
`command.cpp:362,4355` (R194 noted) -- not errors, build succeeds.

## What this protects against

- R193.fix regression: if anyone ever moves
  `add_subdirectory(src/v3/integration/legacy_bnetd)` back to an
  ordering where `bnetd_legacy` is not yet defined,
  `integration_legacy_bnetd_linked` will not be created and the
  bnetd link will fail.
- R194 fix regression: the 5 source fixes + lib extract + init
  wiring all become "compile gates" -- any future change that
  breaks them (e.g. deleting `LegacyBridge::init`, reintroducing
  a 3-arg `conn_destroy` call, or re-gating `prefs_v3_shim.h`)
  will fail this stage.
- General `PVPGN_V3_BNETD_INTEGRATION` strangler-fig regression:
  any new `#ifdef PVPGN_V3_BNETD_INTEGRATION` block that doesn't
  compile under the integration build will fail here.

## Status

- Stage added: `Dockerfile.v3` (+45 lines at end).
- Build verification: `pvpgn-v3-bnetd-on:r195` image green.
- v3-test:r194-final regression image still green (R195 doesn't
  touch the v3-test pipeline).
- No other files changed in R195.

## Out of scope / deferred

- `v3-bnetd-on` is not yet wired into GitHub Actions / appveyor.yml.
  The stage exists and works; calling it from CI is a separate
  trivial change once the repo's CI choice is settled.
- d2cs / d2dbs strangler-fig lanes (Findings beyond R194).
- Runtime / functional smoke (the `CMD` runs `bnetd --help` only).
- Restoring strict warnings on the legacy tree (likely permanent
  deferral).
