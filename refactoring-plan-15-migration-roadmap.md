# 15 · Migration Roadmap (Phased)

The refactor is intentionally non-big-bang. It uses the
**Strangler Fig** pattern: a new code tree is built next to the
legacy one, and modules are migrated one by one with parity gates.

## 0. Pre-work (already partially done)

* C++11 → C++20 toolchain bump.
* Replace `fmt` vendored copy with `FetchContent` pin.
* Add `.clang-format`, `.clang-tidy`, `.editorconfig`.
* Add Catch2 v3 to `tests/` and convert the two existing tests.
* Add CI matrix (gcc/clang × Debug/Release) with sanitizers.
* Add `core/` library skeleton (`Result`, `IClock`, `ILogger`,
  `Endian`).

Outcome: green CI on master, no behaviour change.

---

## Phase 1 — "Stabilise & abstract" (small, safe PRs)

Goal: make the legacy code testable without changing its behaviour.

1. Introduce `core` and replace one utility at a time:
   * `xalloc` → STL containers (drop-in `XAllocCompat` shim during
     transition).
   * `xstr` / `xstring` → `std::string`.
   * `bn_type` byte-swap → `boost::endian`.
   * `scoped_ptr`/`scoped_array` → `std::unique_ptr`.
2. Replace `eventlog()` with `spdlog`-backed `LOG_*` macros (same
   call sites, same output format initially).
3. Introduce `IClock` and route `extern time_t now;` through it
   (singleton SystemClock at first).
4. Move `prefs_*` global accessors behind a `LegacyPrefs` adapter
   reading the typed `ServerConfig` (config file parsing kept
   identical for now).
5. Add an in-process `IEventBus` and emit dual events: legacy code
   paths post events alongside their existing side-effects. Adds new
   subscribers (metrics, webui later) without changing logic.
6. Land a **protocol replay harness**: capture & replay one minimal
   StarCraft login session as a golden test. This is the safety net
   for later phases.

**Definition of done**: legacy bnetd builds and runs identically; the
new `core/` library has unit tests; one protocol replay test passes
in CI; coverage tooling lights up.

---

## Phase 2 — "Network spine on Asio + Fiber"

Goal: replace `fdwatch` with Asio/Fiber without changing higher
layers.

1. Implement `infrastructure/net/io_runtime`, `tcp_acceptor`,
   `tcp_session`, `udp_endpoint`.
2. Provide a `LegacyProtocolHandler` adapter that takes raw bytes,
   builds a legacy `t_packet`, and calls the existing
   `handle_*_packet` functions. Net effect: the network layer
   changes but the handlers don't.
3. Behind a CMake flag `PVPGN_USE_BOOST_FIBER`, switch the live
   reactor over.
4. Add per-session fiber spawn; verify legacy synchronous DB calls
   work (they will block the fiber but not crash).
5. Migrate signal handling to `asio::signal_set`.

**Definition of done**: bnetd runs on Asio/Fiber for 24 h of internal
load test without behavioural regression; metrics for fiber pool and
queue depths exposed.

---

## Phase 3 — "Domain extraction"

Goal: extract aggregates from legacy globals.

For each context (in order: `moderation`, `social`, `chat`,
`gameplay`, `ladder`, `matchmaking`, `realm`, `identity`):

1. Create `domain/<context>/` with aggregate + value objects.
2. Add `application/<context>/` use-cases.
3. Add in-memory repository + tests.
4. Add an adapter under `infrastructure/persistence/legacy_<context>/`
   that reads/writes legacy on-disk format through the new aggregate
   API.
5. Implement a "shadow mode": the legacy handler still owns the
   write path, but each command also runs through the new use-case
   and asserts equality. CI runs this in dev builds; production
   keeps it off until parity is proven.
6. Once shadow mode is clean for N days, switch the **read** path to
   the new repos. The next sprint, switch the **write** path.
7. Delete the legacy module from the build.

`identity` (the `Account` aggregate) goes **last** because it is the
most pervasive, and migrating it last makes every prior context
benefit from typed accounts via the legacy adapter.

**Definition of done per context**: 0 references to legacy
module from new code; replay tests still pass; flag flip in
production.

---

## Phase 4 — "Protocol decoupling"

Goal: turn each `handle_*_packet` into a pure codec + FSM.

1. Per protocol (`bnet`, `irc`, `wol`, `d2cs`, `d2gs`, `file`, `udp`,
   `telnet`):
   a. Implement codec in `protocol/<name>/`.
   b. Add fuzz harness; fix any crashes.
   c. Add a property test: `encode(decode(x)) == x` for valid inputs.
   d. Implement FSM that calls **new** application use-cases.
   e. Switch the `SessionFactory` to instantiate the new FSM behind
      a config flag (`protocol.bnet.implementation = "v3"`).
   f. Run shadow comparison against legacy handler for 1 week.
   g. Default the flag to `"v3"`, then delete `bnetd/handle_<name>.cpp`.

**Definition of done**: all handlers replaced; legacy protocol code
removed; codecs covered by fuzz + replay + property tests.

---

## Phase 5 — "Persistence overhaul"

1. Add SQLite as the new default for fresh installs (`storage.driver = "sqlite"`).
2. Implement schema migrations runner; ship `001_initial.sql` mirroring
   the legacy SQL layout.
3. Provide a one-shot tool `pvpgn-data-migrate file→sqlite` and
   `pvpgn-data-migrate mysql→sqlite`.
4. Rewrite each `sql_<engine>.cpp` as `infrastructure/persistence/<engine>/`
   using parametrised statements and the connection-pool pattern.
5. Deprecate `WITH_ODBC` (kept buildable behind a flag for one major).

**Definition of done**: integration tests pass against SQLite, MySQL,
Postgres; migration tool round-trips a real dataset.

---

## Phase 6 — "Web UI + observability"

1. Land Prometheus exporter (`/metrics`), `spdlog` JSON sink, audit
   pipeline.
2. Land embedded HTTP/WebSocket server (Beast) wired to
   `IoRuntime`.
3. Land WebUI SPA, ship as embedded assets.
4. Replace Win32 GUI with a "Open WebUI" button.
5. Default-enable WebUI on `127.0.0.1:8086` for new installs;
   off for upgrades unless opted in.

**Definition of done**: ops can run a server with zero `bnetd.conf`
editing; admin actions logged in the audit page; Grafana dashboard
green.

---

## Phase 7 — "Scripting & plug-ins"

1. Replace `luainterface*.cpp` with sol3-based host.
2. Ship a compat shim for legacy scripts (`lua/handle_*.lua`).
3. Define plug-in manifest + capabilities.
4. Convert at least one bundled feature (e.g. quiz, ghost) into a
   plug-in to validate the API.

**Definition of done**: bundled `lua/` scripts run unchanged through
compat shim; new plug-in API documented and used by the quiz feature;
sandbox tests pass.

---

## Phase 8 — "d2cs/d2dbs alignment"

1. Migrate d2cs/d2dbs onto `runtime/` (delete duplicated main/cmdline/
   prefs/signal/server).
2. Replace inter-service plaintext links with mutual-TLS-capable
   `PeerLink`.
3. Extract `.d2s` parsing into `protocol/d2save/`; fuzz it.

**Definition of done**: a single `runtime/` library used by all three
binaries; integration test simulates BN ↔ d2cs ↔ d2dbs character
flow.

---

## Phase 9 — "Clean-up & 4.0 prep"

1. Delete `src/legacy/` once unreferenced.
2. Delete unused tools (`bnproxy`, `bnpcap`).
3. Drop Lua 5.1 support; 5.4 only.
4. Drop INI `bnetd.conf` parser; require TOML (with a one-shot
   converter tool).
5. Bump major: `4.0`.

---

## Cross-cutting rules during migration

* **No new code in `src/legacy/`.** Bug fixes flow through the new
  layer where possible; emergency fixes accompanied by a
  ticket to remove the legacy module.
* **Every PR that migrates a module deletes legacy code in the same
  PR.** Prevents drift.
* **Parity gates are mandatory.** A migrated module must pass:
  * Unit tests for the new code.
  * Replay tests for relevant protocols.
  * 1-week shadow mode in canary deploy.
* **Config compatibility**: every legacy config key keeps working
  until a major release; deprecated keys log a warning with the new
  equivalent.
* **Data compatibility**: on-disk flat files keep being read until
  Phase 5; a one-shot upgrade tool then converts.
