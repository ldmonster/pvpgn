# Phase 1 Step 11 -- Final integration and build verification

> Step 10 (TOML migration) wrapped at R162. This file scopes the
> remaining Phase 1 closeout work. Everything here is "polish and
> simplification" -- no new functionality, no behavioural changes for
> deployed servers. Each item is independently shippable.

## Why a Step 11 at all

After R162, the v3 build path is the canonical one for bnetd / d2cs
/ d2dbs:

- TOML is the sole config source under `PVPGN_V3_<svc>_INTEGRATION`.
- Legacy `prefs.cpp` / `prefs.h` no longer compile or link into the
  v3 binaries.
- All call sites go through the `prefs_v3::*` shim, which has a
  static `#ifdef` switch between bridge calls (v3) and legacy
  `prefs_get_*` (non-v3).

But the *non-v3* build path still exists -- the `#else` branches in
every shim accessor still reference symbols from `prefs.cpp`. That
keeps the legacy parser code in the tree and in the non-v3 link.
Phase 1 Step 11 retires the non-v3 build entirely.

## Checklist

### 1. Retire the non-v3 build path

- [ ] **1.1** Decide on a removal window. The non-v3 build has been
      "supported but unused" since R150 (bnetd) / R156 (d2cs) /
      R157 (d2dbs). Propose: drop it now; document in `changelog.md`.
- [ ] **1.2** Flatten `src/{bnetd,d2cs,d2dbs}/prefs_v3_shim.h`:
      remove every `#else` branch. Each accessor becomes a single
      line calling the bridge directly.
- [ ] **1.3** Delete `src/{bnetd,d2cs,d2dbs}/prefs.cpp` and
      `src/{bnetd,d2cs,d2dbs}/prefs.h`.
- [ ] **1.4** Drop the legacy-only entries from each service's
      `CMakeLists.txt` (the `if(TARGET integration_legacy_*_linked)`
      conditional around `prefs.cpp`).
- [ ] **1.5** Remove `PVPGN_BUILD_V3` cmake option as a "choice"
      and make v3 the default + only build. Keep the symbol defined
      for compatibility but flag it as deprecated.
- [ ] **1.6** Stop installing `bnetd.conf`, `d2cs.conf`, `d2dbs.conf`
      from `conf/CMakeLists.txt` (only the `.toml` siblings remain).
      Other supporting `.conf` files (channel, topics, ad, etc.)
      stay -- they are still read by code that has no TOML
      equivalent.

### 2. Operator visibility parity

- [ ] **2.1** d2cs: log the active config snapshot to the eventlog
      on every SIGHUP reload (mirror what `/config` shows for bnetd).
- [ ] **2.2** d2dbs: same.
- [ ] **2.3** Extract the dump formatting into a pure free function
      in `infra/config` so all three services share one
      implementation and get Catch2 coverage for free.
- [ ] **2.4** (Future) When the v3 telnet admin FSM gets a real
      command registry, register `/config` against it so all three
      services have an operator-callable dump.

### 3. Documentation

- [ ] **3.1** Update `docs/compile.*.md` to drop references to the
      legacy `.conf` format.
- [ ] **3.2** Update `README.md` to point at `bnetd.toml` as the
      primary config file.
- [ ] **3.3** Write a one-page migration note (`docs/toml-migration.md`)
      with side-by-side examples for the most common `.conf` -> `.toml`
      transformations. The example deployable files in `conf/*.toml.in`
      already function as the spec, but a narrative helps.
- [ ] **3.4** Add a `changelog.md` entry summarising Step 10 + Step 11.

### 4. CI / build verification

- [ ] **4.1** Confirm `Dockerfile.v3` v3-build stage builds clean
      with `prefs.cpp` files removed (post-1.3).
- [ ] **4.2** Confirm `Dockerfile` (legacy non-v3 image) is either
      retired or pinned to a tagged commit predating Step 11.
- [ ] **4.3** Run the full Catch2 suite under v3 in docker; capture
      the suite count and assertion totals as a baseline for future
      regression detection.

### 5. Stretch (deferred until Step 11 closes)

- [ ] **5.1** Atomic shared_ptr swap for the bridge `g_prefs` -- today
      writes happen on SIGHUP only and reads are unsynchronised. A
      `std::atomic<std::shared_ptr<T>>` upgrade would make hot-reload
      strictly thread-safe even under heavy concurrent reads.
- [ ] **5.2** Investigate whether `prefs_v3::*` should be retired in
      favour of direct `LegacyPrefs` references threaded through the
      call graph. Probably not worth it -- the shim adds no overhead
      and gives us a single audit choke-point.

## Done-when

Step 11 is complete when:

- Repo has no `src/{bnetd,d2cs,d2dbs}/prefs.cpp` or `prefs.h`.
- Every `prefs_v3::*` accessor body is a single bridge call.
- `Dockerfile.v3` v3-test green.
- `changelog.md` documents the deprecation + deletion.

---

## R164 status (2026-05 cont.)

Completed:
- [x] 1.2 prefs_v3_shim.h flattened across all three services
  (bnetd 1151->615, d2cs 505->273, d2dbs 208->120). Single bridge
  call per accessor under PVPGN_V3_*_INTEGRATION; non-v3 fallback
  branch deleted.
- [x] 1.6 conf/CMakeLists.txt now installs only the .toml siblings
  under PVPGN_BUILD_V3 (done in R163).
- [x] 2.1 / 2.2 d2cs / d2dbs SIGHUP reload eventlogs a full TOML
  snapshot via pvpgn_v3_<svc>_prefs_dump (R163).
- [x] 2.3 Shared format_dump() in infra/config/prefs_dump.hpp,
  Catch2 coverage = 4 cases / 46 assertions (R163).
- [x] 3.2 README.md points operators at bnetd.toml and the new
  migration page.
- [x] 3.3 docs/toml-migration.md written; side-by-side examples
  for all three services.
- [x] 3.4 changelog.md prefixed with Step 11 / R163-R164 entry.
- [x] Routed bnetd /config through the same format_dump()
  formatter via a new pvpgn_v3_prefs_dump bridge entry point.

Deferred (R165 candidates):

- [ ] 1.3 / 1.4 delete prefs.cpp + prefs.h. Blocked on:
       * src/bnetd/server.cpp restart_mode_all branch (lines 1714,
         1718) still calls bare prefs_load() with no #ifdef guard.
       * 17 source files still #include "prefs.h" transitively.
       * needs a clean #ifdef PVPGN_V3_BNETD_INTEGRATION sweep over
         the do_restart handler in server.cpp first.
- [ ] 1.5 demote PVPGN_BUILD_V3 to default-on; trivial but should
       follow 1.3/1.4 so the docs/changelog read consistently.
- [ ] 3.1 docs/compile.*.md update -- scan for .conf references.
- [ ] Atomic shared_ptr swap for g_*_prefs in the three bridges
       (R164 stretch). Single-writer/many-reader pattern at the
       moment; SIGHUP reload is theoretically racy with concurrent
       accessors but no observed bug.

Verification:
- docker v3-test green; new prefs_dump suite still reports
  "All tests passed (46 assertions in 4 test cases)".

---

## R164 status (2026-05 cont.)

Completed:
- [x] 1.2 prefs_v3_shim.h flattened across all three services
  (bnetd 1151->615, d2cs 505->273, d2dbs 208->120). Single bridge
  call per accessor under PVPGN_V3_*_INTEGRATION; non-v3 fallback
  branch deleted.
- [x] 1.6 conf/CMakeLists.txt now installs only the .toml siblings
  under PVPGN_BUILD_V3 (done in R163).
- [x] 2.1 / 2.2 d2cs / d2dbs SIGHUP reload eventlogs a full TOML
  snapshot via pvpgn_v3_<svc>_prefs_dump (R163).
- [x] 2.3 Shared format_dump() in infra/config/prefs_dump.hpp,
  Catch2 coverage = 4 cases / 46 assertions (R163).
- [x] 3.2 README.md points operators at bnetd.toml and the new
  migration page.
- [x] 3.3 docs/toml-migration.md written; side-by-side examples
  for all three services.
- [x] 3.4 changelog.md prefixed with Step 11 / R163-R164 entry.
- [x] Routed bnetd /config through the same format_dump()
  formatter via a new pvpgn_v3_prefs_dump bridge entry point.

Deferred (R165 candidates):

- [ ] 1.3 / 1.4 delete prefs.cpp + prefs.h. Blocked on:
       * src/bnetd/server.cpp restart_mode_all branch (lines 1714,
         1718) still calls bare prefs_load() with no #ifdef guard.
       * 17 source files still #include "prefs.h" transitively.
       * needs a clean #ifdef PVPGN_V3_BNETD_INTEGRATION sweep over
         the do_restart handler in server.cpp first.
- [ ] 1.5 demote PVPGN_BUILD_V3 to default-on; trivial but should
       follow 1.3/1.4 so the docs/changelog read consistently.
- [ ] 3.1 docs/compile.*.md update -- scan for .conf references.
- [ ] Atomic shared_ptr swap for g_*_prefs in the three bridges
       (R164 stretch). Single-writer/many-reader pattern at the
       moment; SIGHUP reload is theoretically racy with concurrent
       accessors but no observed bug.

Verification:
- docker v3-test green; new prefs_dump suite still reports
  "All tests passed (46 assertions in 4 test cases)".

## R165 status (2026-05 cont.)

Completed (extension of R164):
- [x] 1.3 / 1.4 prefs.cpp + prefs.h DELETED across bnetd/d2cs/d2dbs.
      src/bnetd/server.cpp restart_mode_all branch (lines ~1714,
      ~1718) gated under PVPGN_V3_BNETD_INTEGRATION and now calls
      pvpgn_v3_prefs_load_toml on SIGHUP-restart. Updated all
      three src/<svc>/CMakeLists.txt to drop the conditional
      list(REMOVE_ITEM ... prefs.cpp) block and the .cpp/.h refs
      from the source list -- bnetd_legacy / d2cs_legacy /
      d2dbs_legacy now unconditionally exclude the legacy parser.
- [x] 1.5 PVPGN_BUILD_V3 demoted to default-on in root CMakeLists.txt
      (option text reworded; PVPGN_BUILD_LEGACY stays default-on as
      the transitional legacy build path).
- [x] 3.1 docs/compile.*.md audit: no .conf references found.
      Updated docs/single-binary-mode.md to use .toml and link to
      toml-migration.md.
- [x] Atomic shared_ptr swap: g_prefs (bnetd), g_d2cs_prefs and
      g_d2dbs_prefs converted to std::atomic<std::shared_ptr<...>>.
      load_toml / unload / dump / accessor macros all take a local
      strong reference via .load() before reading. SIGHUP reload is
      now race-free with concurrent readers (lifetime safety still
      bounded by the accessor call -- see comment in each bridge).
      bnetd bridge: dropped std::optional<LegacyPrefs> + prefs()
      helper in favour of make_shared<LegacyPrefs>(ServerConfig).

Verification:
- docker v3-test green; full suite (prefs_dump still 46/4, infra_log
  still 1225/213, etc.) -- no regressions.

Deferred to R166+:
- Lifetime safety: callers store const char* across reloads which
  may dangle. Fix requires either (a) callers holding shared_ptr
  for the duration of use, or (b) the snapshot's strings being
  immortal once constructed (e.g., interned in a thread-local
  arena). Neither is in scope for Step 11.
- Atomic swap leaves accessor return values valid only inside the
  accessor body. If a caller assigns the return to a long-lived
  variable, a concurrent SIGHUP can still free the underlying
  std::string. The bridges document this constraint.

## R166 status (Step 11 closeout follow-on)

Completed:
- [x] Deleted conf/bnetd.conf.in, d2cs.conf.in, d2dbs.conf.in + their .win32 siblings. conf/CMakeLists.txt simplified -- the v3 build vs legacy build gate around the conf install is gone (v3 is default-on).
- [x] docs/ .conf sweep: bnmotd.md, fdwatch.txt, storage.txt, single-binary-mode.md all reference *.toml now.
- [x] plans/phase3-plan.md written (handler migration roadmap).
- [x] plans/legacy-retirement-scope.md written (PVPGN_BUILD_LEGACY staged retirement L0-L5).
- [x] plans/snapshot-lifetime-scope.md written (residual const char* hazard from atomic swap; four mitigation options).

Verification:
- docker v3-test green; full Catch2 suite passes including infra_log 1225/213 and prefs_dump 46/4.

Step 11 is now closed. Remaining items in this checklist (the L0-L5
legacy retirement, snapshot lifetime safety, etc.) are tracked in
their own scope docs and become Phase 3 / Phase 4 deliverables.
