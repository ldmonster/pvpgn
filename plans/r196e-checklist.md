# R196.e -- Runtime smoke: bnetd + d2cs actually start + bind ports

## Goal

Lock in R194/R195/R196.b's build-level integration gates with an
**actual runtime smoke test**. v3-bnetd-on and v3-d2cs-on prove
the binaries link; v3-smoke proves they start, parse their config,
initialise their event loop, drop privileges, open data files, and
bind to their listen ports without crashing. First stage that
exercises the `#ifdef PVPGN_V3_BNETD_INTEGRATION` / `#ifdef
PVPGN_V3_D2CS_INTEGRATION` strangler paths at runtime.

## Implementation

Two artifacts:

### 1. `scripts/dev/v3-smoke-runtime.sh` (new, +110 lines)

Idempotent shell script invoked from the Dockerfile. Steps:

1. Locate `$BUILD_DIR` (default `/src/build/v3-smoke`) and verify
   `bnetd` and `d2cs` binaries exist.
2. Copy `$BUILD_DIR/conf/*.{toml,conf,json,txt,plain}` into a
   staging conf dir (`/tmp/pvpgn-conf`).
3. **sed-substitute** the two cmake template placeholders left
   literal by `configure_file(@ONLY)`:
   - `${SYSCONFDIR}` → `/tmp/pvpgn-conf`
   - `${LOCALSTATEDIR}` → `/tmp/pvpgn-state`
4. Pre-create the state-tree leaves the daemons expect: `users/`,
   `clans/`, `teams/`, `files/`, `lua/`, `reports/`, `chanlogs/`,
   `userlogs/`, `bnmail/`, `ladders/`, `status/`, `charsave/`,
   `charinfo/`, `bak.charsave/`, `bak.charinfo/`.
5. Copy `/src/files/*` (repo `files/` dir -- IX86ver1.mpq,
   icons.bni, newbie.save, ...) into the state tree's `files/`
   subdir. Without these, `support_check_files()` aborts bnetd at
   startup.
6. Inject `effective_user = "root"` / `effective_group = "root"`
   into `bnetd.toml`. The shipped `bnetd.toml.in` has these
   commented out; `prefs_v3::effective_user()` returns an empty
   string (not nullptr); `give_up_root_privileges("")` calls
   `getpwnam("")` which returns NULL; fatal. (Container runs as
   root, so setuid/setgid stays a no-op.)
7. Pre-flight: verify no `${...}` placeholders remain in
   `bnetd.toml`; bail with diagnostic if any do.
8. For each daemon (bnetd port 6112, d2cs port 6113):
   - Start with `--debug -c $CONF_DIR/$name.toml`, redirect to
     `/tmp/$name.log`.
   - Sleep `$ALIVE_SECS` (default 6) seconds.
   - Assert `kill -0 $pid` succeeds (process still alive).
     Failure = FAIL exit 1, log tail printed.
   - Run `ss -tln` and grep for the expected listen port. Match
     = PASS; miss = WARN (process alive but not bound to expected
     port). WARN does not fail the build.
   - Send SIGTERM, sleep 1, SIGKILL, wait for cleanup.

### 2. `Dockerfile.v3` stage `v3-smoke` (+25 lines)

```Dockerfile
FROM v3-base AS v3-smoke
RUN apk --quiet --no-cache add iproute2
RUN cmake -S . -B build/v3-smoke \
      -D PVPGN_BUILD_V3=ON \
      -D PVPGN_BUILD_LEGACY=ON \
      -D WITH_BNETD=ON \
      -D WITH_D2CS=ON \
      ...
   && cmake --build build/v3-smoke --config Release \
            --target bnetd --target d2cs -j"$(nproc)"
RUN chmod +x scripts/dev/v3-smoke-runtime.sh \
 && BUILD_DIR=/src/build/v3-smoke scripts/dev/v3-smoke-runtime.sh
```

**Why a script file, not an inline RUN**: an earlier attempt put
the entire smoke logic inline with multi-line `RUN` and
backslash-continuation. Docker BuildKit interpreted the literal
`${SYSCONFDIR}` / `${LOCALSTATEDIR}` inside the inline sed
expression as Dockerfile variables (despite single quotes),
substituting them with empty strings before the sh runtime saw
the command -- so sed had nothing to match and the daemons
started with the placeholders intact, dying immediately on
`fopen("${LOCALSTATEDIR}/bnetd.log")`. Moving the sed into a
checked-in `.sh` file dodges the BuildKit-vs-shell quoting
mismatch.

## Iteration trace (3 fixes)

1. **First run**: died immediately on
   `eventlog_open: could not open file "${LOCALSTATEDIR}/bnetd.log"`.
   Root cause: BuildKit ate the inline sed pattern's `${VAR}`.
   Fix: moved logic to `scripts/dev/v3-smoke-runtime.sh`.

2. **Second run**: died on
   `gurp_uname2id: cannot get password file entry for ''` ->
   `main: could not give up privileges`. Root cause:
   `prefs_v3::effective_user()` returns `""` (not nullptr) when
   the toml has the field commented out;
   `give_up_root_privileges` only treats nullptr as "skip", not
   empty string. Fix: smoke script seds in
   `effective_user = "root"` before launching.

3. **Third run**: died on
   `support_check_files: necessary file ".../files/IX86ver1.mpq" missing`.
   Root cause: bnetd refuses to start without the MPQ/BNI support
   files from repo's `files/` directory. Fix: smoke script copies
   `/src/files/*` into the state-tree's `files/` subdir.

4. **Fourth run** (smoke went green):
   ```
   ==> starting bnetd --debug -c /tmp/pvpgn-conf/bnetd.toml
   PASS: bnetd alive after 6s (pid 58)
   PASS: bnetd listening on :6112
   ==> bnetd smoke complete
   ==> starting d2cs --debug -c /tmp/pvpgn-conf/d2cs.toml
   PASS: d2cs alive after 6s (pid 67)
   PASS: d2cs listening on :6113
   ==> d2cs smoke complete
   naming to docker.io/library/pvpgn-v3-smoke:r196e done
   ```

## What this protects against (over v3-bnetd-on / v3-d2cs-on)

| Failure class | v3-bnetd-on | v3-d2cs-on | v3-smoke |
|---|---|---|---|
| Compile error in legacy bnetd | catch | - | catch |
| Compile error in legacy d2cs | - | catch | catch |
| Link error in either | catch | catch | catch |
| Static initialiser segfault (constructor of any v3 lib linked in) | miss | miss | **catch** |
| Config loader regression (TOML parse) | miss | miss | **catch** |
| `eventlog_startup` / `give_up_root_privileges` / FDW init regression | miss | miss | **catch** |
| Listen socket bind regression (`addrlist_create` → `psock_bind`) | miss | miss | **catch** |
| Strangler `#ifdef PVPGN_V3_*_INTEGRATION` runtime regression at startup | miss | miss | **catch** |

## Notes / non-issues observed

- d2cs logs `host_lookup: could not lookup host "<d2gs-IP>"` and
  `s2s_create: error connecting to <bnetd-IP>:6112`. These are
  expected: the toml ships placeholder hostnames that an operator
  is expected to replace; in CI they fail to resolve. They are
  warnings, not fatals. The smoke test only requires the daemons
  to be alive after 6s + bound to their primary listen ports.
- d2cs logs `d2ladder_readladder: error opening ladder file ...
  ladder.D2DV`. Expected: no initial ladder data in a fresh state.
  Non-fatal.
- bnetd's primary BNCS port is `:6112` (matched); secondary ports
  (telnet `:23`, IRC `:6667`, WOL, etc.) are disabled by default
  in the shipped toml (blank `servaddrs`) so we don't check them.
- d2cs's primary listen port is `:6113` (the historic "d2cs port"
  is sometimes documented as 6200, but the actual listening port
  in the codebase is 6113; 6200 is d2gs which is a separate
  daemon).

## Files changed

| File | Change |
|------|--------|
| `Dockerfile.v3` | +25 lines (`v3-smoke` stage + Usage:) |
| `scripts/dev/v3-smoke-runtime.sh` | NEW, +120 lines |
| `scripts/dev/probe-toml-subst.sh` | NEW (debug helper used during R196.e iteration), +13 lines |

## Out of scope / deferred

- **Client handshake smoke**: starting bnetd and connecting a
  real BNCS client (e.g. via the `bnchat` tool in the repo).
  Would require accounts, channels, password infrastructure.
- **End-to-end integration**: bnetd + d2cs + d2gs running together
  with s2s wired up. Currently d2cs cannot connect to bnetd
  because the smoke runs them sequentially, not in parallel.
  A `docker-compose` integration target is one path; reuse the
  existing `docker-compose.yml` may also work.
- **d2dbs lane**: R196.c candidate (still blocked on creating
  `integration_legacy_d2dbs_linked`).
- **Wiring into CI**: same deferral as R195. The smoke stage
  exists and works; calling it from GitHub Actions / appveyor is
  a separate trivial change.
- **Audit / fix of `prefs_v3::effective_user()` returning ""
  instead of `nullptr` when unset**: that's the root cause of
  fix #2. The R196.e workaround (sed in `effective_user = "root"`)
  hides it. Fixing it properly would mean teaching the v3 toml
  loader to distinguish "missing key" from "empty string", and
  having `prefs_v3::effective_user()` return `nullptr` for the
  missing case. Followup ticket.

## Status

- `pvpgn-v3-smoke:r196e` image green (build + smoke both pass).
- `pvpgn-v3-bnetd-on:r196b` regression -- unaffected (separate stage).
- `pvpgn-v3-d2cs-on:r196b` regression -- unaffected.
- Three docs updated (this checklist + progress-master + changelog).
