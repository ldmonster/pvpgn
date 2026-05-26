# L1 cutover scoping (R167) — Dockerfile.v3 production target

> Per `plans/legacy-retirement-scope.md` stage L1: shift the
> production docker image from running legacy `bnetd` to running
> `pvpgn_v3_bnetd`. This is a scoping note; no Dockerfile changes
> in R167.

## Current state

`Dockerfile.v3` builds three named stages:

- `v3-base`   — alpine + toolchain (cmake 4, gcc 14, Boost).
- `v3-build`  — `cmake --build` of Catch2 **test** targets only.
- `v3-test`   — runs the test binaries.
- `v3-e2e`    — end-to-end smoke harness.

Notably absent: a stage that builds and starts `pvpgn_v3_bnetd` as a
long-running service. The legacy `Dockerfile` (without `.v3` suffix)
is what production currently runs.

## Prerequisites before flipping production

1. **Functional feature parity.** `pvpgn_v3_bnetd` must accept BNet,
   FILE, BOT, TELNET, and D2CS_BNETD connections. Currently the v3
   handler set is dispatch-ready but `app/bnetd` composition root
   needs Phase 3.A `install_init_conn_apply_handler` wired
   unconditionally. (See `plans/phase3a-handle-init-audit.md`.)
2. **Account storage backend.** The v3 binary uses the same
   `prefs_v3::storage_path()` pointer; underlying file/SQL drivers
   are still legacy. Confirm v3 startup actually loads accounts.
3. **Telnet `/config` admin command.** Works today via legacy
   `command.cpp::_handle_config_command` -- but that file is bnetd
   legacy. Confirm the v3 binary exposes an equivalent admin path.
4. **Signal handling.** SIGHUP / SIGTERM / SIGUSR1/2 must do the
   right thing in `pvpgn_v3_bnetd::main.cpp`. Audit needed.
5. **Healthcheck.** Add `--help` smoke (R167) is fine for
   image-build verification, but production needs `HEALTHCHECK
   CMD nc -z localhost 6112` or similar.

## Recommended L1 staged plan

| Sub-stage | Action                                                                                       |
| --------- | -------------------------------------------------------------------------------------------- |
| L1.a      | Add a `v3-runtime` stage to `Dockerfile.v3`: builds + installs `pvpgn_v3_bnetd`, sets `CMD ["/usr/local/bin/pvpgn_v3_bnetd","--config","/etc/pvpgn/bnetd.toml"]`. |
| L1.b      | Wire the existing E2E harness against `v3-runtime` instead of the legacy bnetd to verify the cutover end-to-end. |
| L1.c      | Update `docker-compose.yml` (top-level) to use the new image tag. Keep legacy image alongside for one release. |
| L1.d      | Ship a release notes entry; one cycle of production observability. |
| L1.e      | Delete the legacy `Dockerfile` and `docker-compose.yml` legacy service entry. |

## R167 deliverable

This scoping note + the `--help` / `--version` smoke tests (already
landed in `src/v3/app/bnetd/CMakeLists.txt` and
`src/v3/app/d2cs/CMakeLists.txt`). Actual `v3-runtime` stage is
deferred to a later round once Phase 3.A (R168/R169) lands.
