# Refactoring Progress

Live tracker for the implementation of [refactoring-plan-00-overview.md](refactoring-plan-00-overview.md).
Phases are taken from [refactoring-plan-15-migration-roadmap.md](refactoring-plan-15-migration-roadmap.md).

Legend: `[ ]` not started · `[~]` in progress · `[x]` done · `[!]` blocked

---

## Phase 0 — Pre-work

- [x] **Toolchain & repo hygiene**
  - [x] `.editorconfig`
  - [x] `.clang-format`
  - [x] `.clang-tidy`
  - [ ] `.cmake-format.yaml` (deferred — non-blocking)
  - [ ] `.pre-commit-config.yaml` (deferred — non-blocking)
- [x] **CMake modernisation (additive)**
  - [x] `CMakePresets.json`
  - [x] `cmake/v3.cmake` helper module
  - [x] Root option `PVPGN_BUILD_V3` (default OFF; legacy build unchanged)
  - [x] `src/v3/CMakeLists.txt` with C++20 sub-tree
- [x] **`core` library skeleton (header-only first)**
  - [x] `core/result.hpp` (Result<T,E>, expected-like, monadic map/and_then/map_error)
  - [x] `core/error.hpp` (StatusCode, `Error` value type)
  - [x] `core/strong_typedef.hpp` (StrongTypedef + StrongId + std::hash)
  - [x] `core/bytes.hpp` (ByteSpan/ByteView + hex en/decode)
  - [x] `core/endian.hpp` (read_le/read_be/write_le/write_be with OutOfRange check)
  - [x] `core/clock.hpp` (IClock + SystemClock + ManualClock)
  - [x] `core/logging.hpp` (ILogger + NullLogger + StreamLogger + default sink)
  - [x] `core/version.hpp` (semver constants)
- [x] **Test harness**
  - [x] Catch2 v3 via `FetchContent` (3.5.4)
  - [x] Catch2 headers marked SYSTEM (so v3 strict warnings don't fire on them)
  - [x] `tests/unit/core/` with 25 test cases — **all passing**
- [x] **Verified: both builds are green side-by-side**
  - v3-only: `cmake -S . -B build/v3 -DPVPGN_BUILD_V3=ON -DPVPGN_BUILD_LEGACY=OFF` → 25/25 tests pass.
  - Legacy: `cmake -S . -B build/legacy` (defaults) → bnetd, d2cs, d2dbs, bntrackd, bnpass, client tools, bniutils all build.
- [ ] **CI scaffolding** — left as a separate later step.

## Phase 1 — Stabilise & abstract
- [x] Boost dependency wiring (system Boost 1.83 via `find_package`, header-only Asio; Fiber gated by `PVPGN_V3_WITH_FIBER`)
- [x] `infra/log` — spdlog adapter (`SpdlogLogger`, `make_spdlog_logger`)
- [x] `infra/config` — toml++ adapter with typed `ServerConfig`
- [x] `infra/config/legacy_prefs.hpp` — `LegacyPrefs` adapter mirroring legacy `prefs_get_*` accessors on top of `ServerConfig`
- [x] `core/event_bus.hpp` — in-process `IEventBus` (RAII subscriptions, type-keyed channels)
- [x] `core/scheduler.hpp` — `IScheduler` + `ManualScheduler` (Asio-backed prod impl is Phase 2)
- [x] `core/format.hpp` — `std::format`-based `LOG_TRACE..LOG_CRITICAL` macros
- [x] Protocol replay harness — generic `protocol::replay<Decoded>(stream, decode)` + golden test against bnet codec
- [ ] `IClock` routing into legacy `extern time_t now;` (deferred — strangler-fig: legacy stays untouched)
- [ ] xalloc → STL, xstr → std::string, scoped_ptr → unique_ptr (deferred to per-module sweeps per plan §15)
- [ ] eventlog() → `LOG_*` rewrite at call sites (deferred to per-module migrations)

## Phase 2 — Network spine (Asio + Fiber)
- [x] Boost wiring — `find_package(Boost 1.75 REQUIRED COMPONENTS system [fiber context])` behind `PVPGN_V3_WITH_BOOST` (default ON)
- [x] `infra/net/io_runtime` — owns `boost::asio::io_context` + N worker threads; `run/stop`; `post`; `signal_set` for graceful shutdown
- [x] `infra/net/tcp_acceptor` — bind/listen + accept loop, hands sockets to a `SessionFactory`; bound-endpoint result for port-0 tests
- [x] `infra/net/tcp_session` — `shared_from_this`, strand-serialised read/write, deque-backed write queue, `on_bytes`/`on_close` callbacks
- [x] `infra/net/fiber.hpp` — optional Boost.Fiber awaiter helpers gated by `PVPGN_V3_HAVE_FIBER` (flag `PVPGN_V3_WITH_FIBER`, default OFF)
- [x] End-to-end echo integration test on loopback
- [x] `infra/net/udp_endpoint` — bind/recv/send_to + strand-serialised send queue; `adopt_native_handle()` to reuse legacy-opened sockets
- [x] `LegacyProtocolHandler` adapter — framing for all legacy connection classes (Init/Bnet/File/D2csBnetd/W3route/WolGameres/Bot/Telnet/Irc/Wol/Wladder) with virtual `dispatch_frame()` hook
- [x] `LegacyUdpDispatcher` — builds legacy `t_packet` and dispatches to `pvpgn::bnetd::handle_udp_packet` (TCP `dispatch_frame` still seam-only; needs `t_connection*` plumbing)
- [x] `UdpBridge` — owns IoRuntime + UdpEndpoints + dispatchers; wired into `bnetd` exe (UDP fdwatch loop replaced)
- [x] Per-session fiber spawn behind `PVPGN_V3_WITH_FIBER` — `SessionChannel` + `spawn_session()` + `FiberPool` (multi-thread fan-out with `asio::round_robin` integration)
- [ ] TCP `dispatch_frame` real wiring (needs `t_connection*` construction from outside legacy composition root)
- [ ] Replace bnetd TCP `fdwatch` accept loop with `TcpAcceptor`

## Phase 3 — Domain extraction
- [x] `domain/shared/ids.hpp` — `AccountId`, `ChannelId`, `GameId`, `ClanId`, `TeamId` (StrongId-typed)
- [x] `domain/shared/client_tag.hpp` — `ClientTag` validated 4-byte ASCII tag (`STAR`, `D2DV`, …)
- [x] `domain/shared/user_name.hpp` — `UserName` validated (2..15, alnum/`_-.`, starts with letter, case-insensitive equality, case-preserving display)
- [x] `domain/shared/locale.hpp` — `Locale` 4-char BNet locale with `enUS` fallback
- [x] `domain/shared/bn_hash.hpp` — `BNHash` 20-byte BNet password hash with constant-time equality
- [x] `domain/shared/ip_address.hpp` — IPv4 dotted-quad + IPv6 full-form parser, pure (no syscalls)
- [x] `domain/shared/ban.hpp` — `Ban` value object with `active_at(now)` predicate
- [x] `domain/shared/chat_message.hpp` — bounded validated UTF-8 body (1..223, no control chars)
- [x] `domain/shared/match_report.hpp` — `MatchOutcome`/`PlayerResult`/`MatchReport` value objects
- [x] `domain/shared/events.hpp` — `DomainEvent` variant with **40 alternatives** (identity, chat, social, gameplay, moderation, matchmaking, realm, attributes)
- [x] `domain/identity/account.hpp` — `Account` aggregate (create/rehydrate, login with ban+lock gating, change_password, command-group grant/revoke, apply_ban/clear_ban, drain_events)
- [x] `domain/identity/attribute_map.hpp` — `AttributeMap` typed wrapper around the legacy `BNET\acct\*` bag (idempotent set, change-event emission, rehydrate)
- [x] `domain/chat/channel.hpp` — `Channel` aggregate (admit/leave/post/kick/set_topic; ChannelFlags bitset; ChannelPolicy with max_members + client-tag restriction; idempotent join; kick → banlist; AccountId-keyed members)
- [x] `domain/social/friend_list.hpp` — `FriendList` aggregate (add/remove, 25-cap, self-rejection, idempotent add)
- [x] `domain/social/clan.hpp` — `Clan` aggregate (create with validated 2..4 tag, founder seeded as Chieftain, ranks Chieftain/Shaman/Grunt/Peon, 250-cap, join/remove/set_rank)
- [x] `domain/social/team.hpp` — `Team` aggregate (W3 arranged-team, fixed 2..4 unique members, immutable roster, one-way disband)
- [x] `domain/gameplay/game.hpp` — `Game` aggregate + deterministic FSM (`Open → InProgress → Reporting → Finalized`); host/join/leave/start/begin_report/finalize; emits `MatchReport` on finalize
- [x] `domain/ladder/ladder.hpp` — `LadderCalculator` stateless service (Elo with configurable K-factor, per-player opponent-mean, disconnect-as-loss toggle)
- [x] `domain/moderation/ip_ban_list.hpp` — `IpBanList` aggregate (exact-IP + CIDR ranges; expiry-aware `blocks()`; `prune_expired()` covers both)
- [x] `domain/moderation/quota.hpp` — `Quota` sliding-window rate limiter (Allowed/Throttled/Muted; mute auto-lifts; emits `AccountQuotaExceeded`)
- [x] `domain/matchmaking/anon_game_queue.hpp` — `AnonGameQueue` FIFO matchmaking (idempotent enqueue, `match(GameId)` pulls `2*team_size` oldest, emits `AnonGameMatched`)
- [x] `domain/matchmaking/tournament.hpp` — `Tournament` single-elim scheduling (≥2 participants, emits `TournamentScheduled`)
- [x] `domain/realm/realm.hpp` — `Realm` aggregate + `Character` value (D2 realm catalog; 16-char names; case-insensitive uniqueness; one-way `unregister`)
- [ ] AttributeMap typed accessors per legacy key family — next (currently raw `string→string`)
- [ ] Per-context repository interfaces (`IAccountRepo`, …) — Phase 5 prep

## Phase 4 — Protocol decoupling
- [x] `protocol/common/packet.hpp` — `BnetHeader`, `parse_bnet_header`, `parse_packet` (bounded, non-throwing)
- [x] `protocol/common/reader.hpp` — `Reader` (LE/BE int, NUL-string, raw blob, skip; non-advancing on OOB)
- [x] `protocol/common/writer.hpp` — `Writer` with `begin_bnet_packet` / `finalize_bnet_packet` size back-patch
- [~] per-protocol codecs
  - [~] `protocol/bnet/` — pure codec covers **SID_NULL (0x00), SID_ENTERCHAT (0x0A), SID_JOINCHANNEL (0x0C), SID_CHATCOMMAND (0x0E), SID_CHATEVENT (0x0F), SID_PING (0x25), SID_LOGONRESPONSE2 (0x3A) both directions, SID_AUTH_INFO (0x50), SID_AUTH_CHECK reply (0x51)** with full round-trip tests
  - [x] `protocol/irc/` — RFC 1459 framer + tokeniser + encoder (handles bare-LF, prefix/command/middle/trailing, upper-cases commands, `Message` value type)
  - [ ] remaining BNet SIDs (game-list, ladder, file-transfer init, AUTH_CHECK client direction, …)
  - [x] `protocol/udp/` — connection-less codec for UDPTEST (0x05), UDPPING (0x07), SESSIONADDR1/2 (0x08/0x09); `Datagram` variant
  - [x] `protocol/telnet/` — line framer (CRLF or bare LF), `tokenise(line) → Command{verb,args}`, `write_line()` reply helper
  - [x] `protocol/file/` — BNFTP header (u16 size + u16 type) + `ClientFileReq` (0x0100) and `ServerFileReply` (0x0000) round-trip
  - [x] `protocol/d2cs/` — 3-byte header (u16 size + u8 type) + D2CS LoginReq / LoginReply (0x01) round-trip
  - [x] `protocol/d2gs/` — 8-byte header (u16 size + u16 type + u32 seqno) + SetGsInfo / Echo / Control with direction-tagged variants
  - [x] `protocol/wolgameres/` — BE TLV walker (u16 size + u16 rngd_size, optional 4-byte zero prefix, repeated u32-tag/u16-type/u16-len TLVs)
  - [x] D2CS realm messages (create-char/create-game/join-game, both directions)
  - [x] D2GS AUTHREQ (0x10) + direction-tagged AUTHREPLY (0x11) family
  - [ ] D2CS game-list / game-info (0x05/0x06), D2GS chat / move / itemdrop bulk-state messages
- [~] per-protocol FSMs
  - [x] `protocol/bnet/fsm.hpp` — `BnetFsm` (states `Init →
    AuthInfoReceived → LoggedIn → InChat → Closing`, plus `Closing`
    on any out-of-order packet); injects `ISessionContext` for I/O;
    canonical replies wired (PING echo, AUTH_CHECK ack,
    LOGONRESPONSE2 ack, ENTERCHAT echo, JOINCHANNEL → CHATEVENT
    EID_CHANNEL, CHATCOMMAND → EID_TALK)
  - [x] `protocol/irc/fsm.hpp` — `IrcFsm` (Greeting → Registered →
    InChannel → Closing); injects `ISessionContext` with
    `server_name()`; emits canonical 001 RPL_WELCOME, 421/431/451/461
    error numerics, PONG, JOIN echo + 366 RPL_ENDOFNAMES, QUIT close
  - [ ] fuzz harnesses + replay golden tests — next

## Phase 5 — Persistence overhaul
- [ ]

## Phase 6 — WebUI + observability
- [ ]

## Phase 7 — Scripting & plug-ins
- [ ]

## Phase 8 — d2cs/d2dbs alignment
- [ ]

## Phase 9 — Clean-up & 4.0 prep
- [ ]

---

## Change log

(entries appended bottom-up by date; newest last)(entries appended bottom-up by date; newest last)

### 2026-05-12 — Asio↔Fiber scheduler integration (round_robin)

Closes the gap left by cont. 14: `spawn_session` now actually works
on a real Asio worker thread.

#### What changed

1. New header
   `src/v3/infra/net/include/infra/net/asio_round_robin.hpp` —
   vendored from Boost.Fiber's `examples/asio/round_robin.hpp`
   (Boost 1.83.0, BSL-1.0, © Oliver Kowalke 2013). Trimmed to drop
   the `yield.hpp` include (we never use the asio yield_t completion
   token); behaviour is unchanged. Wrapped in a GCC pragma block
   that silences the example's pedantic warnings.

2. `IoRuntime::run` extended:
   ```cpp
   void run(std::size_t threads = 1,
            bool        install_fiber_scheduler = false);
   ```
   When `install_fiber_scheduler == true && PVPGN_V3_HAVE_FIBER`,
   each (single, see below) worker thread installs
   `boost::fibers::asio::round_robin` as its scheduling algorithm
   before entering `ctx_.run()`. The aliasing-`shared_ptr` trick
   gives `round_robin` the `shared_ptr<io_context>` it wants without
   transferring ownership.

   Restriction: round_robin's `io_context::service` is per-context,
   so the runtime forces `threads = 1` when the flag is set. This is
   documented in the header.

3. Default value preserves source compatibility: every existing
   `rt.run(N)` call site continues to work unchanged.

#### Test

`tests/unit/infra/net/fiber_session_test.cpp` gains a 4th case,
`spawn_session: echoes loopback bytes via round_robin scheduler` —
a real loopback TCP echo where:

  * The acceptor accepts on a fiber-scheduled IoRuntime.
  * Each accepted session is wired via `spawn_session(...)` to a
    synchronous read-loop handler.
  * A separate `asio::io_context` in the test thread connects,
    writes, and reads the echo back.

Confirms that bytes pushed into `SessionChannel` from the network
side genuinely wake the blocked fiber on the worker thread.

#### Build matrix

| Build dir         | Flags                                            | Tests |
|-------------------|--------------------------------------------------|-------|
| `build/v3`        | `-DPVPGN_BUILD_V3=ON`                            | 226/226 |
| `build/v3-fiber`  | `…+ -DPVPGN_V3_WITH_FIBER=ON`                    | 230/230 |
| `build/combined`  | `-DPVPGN_BUILD_LEGACY=ON -DPVPGN_BUILD_V3=ON`    | 226/226 |
| `build/legacy`    | `-DPVPGN_BUILD_LEGACY=ON`                        | green |

#### Remaining deferred

- Replace bnetd's UDP `fdwatch` path with `UdpEndpoint` +
  `LegacyUdpDispatcher` in `src/bnetd/main.cpp`.
- TCP `handle_*_packet` integration (requires `t_connection*`
  construction, which needs the full legacy server composition root).
- Multi-threaded fiber pool (one io_context per worker would be the
  canonical pattern).

### 2026-05-12 — Multi-threaded FiberPool

Closes the last item from the fiber arc: a real multi-thread fiber
runtime, since the single-context `round_robin` is constitutionally
single-threaded.

#### What changed

New header + impl:

  * `src/v3/infra/net/include/infra/net/fiber_pool.hpp`
  * `src/v3/infra/net/src/fiber_pool.cpp`

Added to `infra_net` sources unconditionally; the body is gated on
`PVPGN_V3_HAVE_FIBER` so non-fiber builds compile it as an empty
TU.

#### Architecture

  * `FiberPool` owns N independent `boost::asio::io_context`s, one
    per worker thread. Each worker installs its own `round_robin`
    scheduler before entering `ctx.run()`.
  * `start(N)` is idempotent. `stop()` drops work guards, halts each
    context, joins threads. Destructor calls `stop()`.
  * `next_executor()` returns a round-robin pick of a worker
    executor for callers who want to manually place timers/sockets.
  * `accept(host, port, handler, [inbox_capacity])` listens on
    worker 0, and on each accepted socket:
      1. Picks a target worker round-robin.
      2. Migrates the OS file descriptor from the worker-0-bound
         socket onto the target worker's executor (release native
         handle → `assign()` on a fresh `tcp::socket`).
      3. Posts the session-creation lambda to the target's executor
         so all session work — TcpSession construction,
         on_bytes/on_close wiring, fiber spawn, `start()` — happens
         on the worker that will run it.
  * Sessions are *pinned* to their worker for life. No cross-worker
    migration, no work-stealing. Documented in the header.

#### Tests

`tests/unit/infra/net/fiber_pool_test.cpp` — 3 cases:

  * **2 workers serve 8 concurrent loopback echos** — the meat: 8
    OS threads each open their own client, write `"client-N"`, read
    it back, and only count success when round-trip matches.
    Asserts `successes == 8`.
  * **accept fails before start** — returns `FailedPrecondition`.
  * **invalid bind address** — returns `InvalidArgument`.

Wired in `tests/unit/infra/net/CMakeLists.txt` under the existing
`if(PVPGN_V3_WITH_FIBER)` block.

#### Build matrix

| Build dir         | Flags                                            | Tests |
|-------------------|--------------------------------------------------|-------|
| `build/v3`        | `-DPVPGN_BUILD_V3=ON`                            | 226/226 |
| `build/v3-fiber`  | `…+ -DPVPGN_V3_WITH_FIBER=ON`                    | 233/233 |
| `build/combined`  | `-DPVPGN_BUILD_LEGACY=ON -DPVPGN_BUILD_V3=ON`    | 226/226 |
| `build/legacy`    | `-DPVPGN_BUILD_LEGACY=ON`                        | green |

#### Honest scope notes

  * One acceptor per `FiberPool::accept()` call, lives on worker 0.
    For massive accept throughput a per-worker `SO_REUSEPORT`
    listener pool would be the next refinement.
  * Native-fd migration uses the platform `int` descriptor under
    POSIX. Windows is untested (the path goes through Asio's
    `native_handle_type` → it should "just work" but isn't
    exercised in CI yet).
  * Fiber lifetime tied to handler return; no kill switch on
    individual fibers (close the session ⇒ recv() returns nullopt
    ⇒ handler exits naturally).

#### Remaining deferred

  * Replace bnetd's UDP `fdwatch` path with `UdpEndpoint` +
    `LegacyUdpDispatcher` in `src/bnetd/main.cpp`.
  * TCP `handle_*_packet` integration (requires `t_connection*`
    construction, which needs the full legacy server composition
    root).
  * Per-worker `SO_REUSEPORT` listeners.

## 2026-05-13 — Real strangler-fig cut: v3 owns bnetd UDP

First production-path swap: the legacy `bnetd` executable now
delegates *all* UDP receive I/O to the v3 networking stack (Asio
`UdpEndpoint`) and forwards each datagram to the legacy
`handle_udp_packet` via `LegacyUdpDispatcher`. The legacy
`fdwatch`-driven UDP read loop is gated off by a new server-side
flag.

### Changes

* `infra/net/udp_endpoint`: new `adopt_native_handle(native_handle_t)`
  API. Takes ownership of an already-bound OS socket, queries its
  local endpoint, and starts the receive pump. Lets us reuse the
  exact socket the legacy `net_udp_listen` opened (correct
  bind/reuse semantics, no port re-grab race).

* `src/bnetd/server.h` / `server.cpp`:
  - New static `udp_listener_fds_` vector populated as each UDP
    listener is created in `sd_create()`.
  - New static `skip_udp_fdwatch_` flag (default false). When set
    before `server_process()`, the UDP sockets are *not* added to
    `fdwatch` and the legacy UDP poll path is skipped.
  - Public getters `udp_listener_fds()` /
    `skip_udp_fdwatch()`, and setter
    `set_skip_udp_fdwatch(bool)` exposed in the `pvpgn::bnetd`
    namespace.

* `src/v3/integration/legacy_bnetd/udp_bridge`: new helper that
  owns an `IoRuntime` (1 worker thread) plus N `UdpEndpoint`s and
  N `LegacyUdpDispatcher`s. `attach(fds)` adopts every legacy UDP
  fd; `stop()` is idempotent and shuts the I/O thread cleanly.

* `src/bnetd/main.cpp`: composition-root wiring. Before
  `server_process()` is called, we call
  `bnetd::set_skip_udp_fdwatch(true)`, then after the listeners
  are created we instantiate `UdpBridge` and `attach()` the fds.
  On shutdown, `bridge.stop()` runs before the legacy server tear
  down.

* CMake: `bnetd` exe now links `integration_legacy_bnetd_linked`
  privately in the combined build (still no-op when v3 is off —
  guarded by `if(TARGET integration_legacy_bnetd_linked)`).

### Why UDP first

* No `t_connection*` is needed for `handle_udp_packet` — it takes
  a raw socket + addr/port + packet.
* Legacy UDP only carries the BNCS port-check / NAT-traversal
  protocol — small, well-isolated traffic with established tests.
* Reusing the legacy-opened fd means zero behaviour change for
  operators: same bind address, same port-reuse policy, same
  multi-bind support, same firewall holes.

### Build & test gates

| Build dir         | Flags                                            | Tests |
|-------------------|--------------------------------------------------|-------|
| `build/v3`        | `-DPVPGN_BUILD_V3=ON`                            | 227/227 |
| `build/v3-fiber`  | `…+ -DPVPGN_V3_WITH_FIBER=ON`                    | 234/234 |
| `build/combined`  | `-DPVPGN_BUILD_LEGACY=ON -DPVPGN_BUILD_V3=ON`    | 227/227 |
| `build/legacy`    | `-DPVPGN_BUILD_LEGACY=ON`                        | green |

The `bnetd` binary in `build/combined/src/bnetd/bnetd` now contains
the v3 UdpBridge code (verified at link time). Runtime smoke-test
of the swapped path requires a full server stand-up (config,
storage, eventlog), which is out of scope for the unit-test gate
— next step.

### Test coverage added

* `tests/unit/infra/net/udp_adopt_test.cpp` — UDP fd adopt loopback
  round-trip + InvalidArgument on bad fd.

### Honest scope notes

* TCP `handle_*_packet` integration still deferred (needs
  `t_connection*` construction, which entangles the full legacy
  composition root).
* Runtime smoke test against a real client (e.g. WAR3 BNCS
  port-check) is pending — the swap is link-clean and unit-clean
  but hasn't been exercised end-to-end yet.
* The `UdpBridge` runs a single Asio worker thread; UDP volume is
  low enough that this is fine, but the `FiberPool` machinery is
  available for any future protocol that needs N-thread fan-out.


---

> **NOTE (recovery):** The progress notes for Batches 23 through 38e
> were lost mid-session due to a string-replace edit that matched and
> truncated a much larger region than intended. The code changes for
> those batches are preserved in the working copy (see git diff vs
> HEAD); only the narrative changelog entries were lost. Earlier
> entries can be reconstructed from the chat transcript at
> `c:\Users\user\AppData\Roaming\Code\User\workspaceStorage\c4a9fc88ae796fd69b6b387dfb541fdd\GitHub.copilot-chat\transcripts\9be30c7d-ada1-4109-a20a-81d4f432e754.jsonl`
> if needed. The two entries below cover only the most recent work.

## 2026-06-21 (oo) -- Batch 38f: TcpBridge live flip (compilation-validated only)

### Caveat

The agent cannot run a real BNet/D2DV client against the server, so
this batch ships the structural flip and asserts only that:

1. all three build matrices (v3 / legacy / linked) compile clean
   under `/WX`,
2. the existing test suites (694 / 698 / 712 cases) pass at 100%,
3. the default code path (`v3_tcp_session_mode = 0`) is
   byte-for-byte identical to pre-38f behaviour.

End-to-end validation with `v3_tcp_session_mode = 1` is an
operator responsibility; runtime gaps are tracked at the end of this
entry.

### What this batch lands

* **Class-refresh hook on `LegacyBnetFrameRouter`.** New
  `set_class_refresh` / `clear_class_refresh` statics in the
  router header. After every successful dispatch the router consults
  the hook and calls `set_class(...)` so the framing follows the
  legacy conn's class transition (`conn_class_init` ->
  `conn_class_bnet` after the magic byte).
* **Init-byte handling in the linked-variant dispatch.** Dispatches
  on `conn_get_class(conn)`: `conn_class_init` ->
  `handle_init_packet`; `conn_class_bnet` -> `handle_bnet_packet`;
  other classes log Warn + return failure.
* **Class-refresh registration.** `refresh_class_from_conn` maps
  every relevant `conn_class_*` to `ConnectionClass`; the
  `AutoRegister` ctor installs it.
* **Owned-socket factory on the legacy side.**
  `server_handle_v3_owned_bnet_socket` in `src/bnetd/server.cpp`
  + declaration in `server.h`. Mirrors `sd_finalize_accepted` but
  skips `PSOCK_NONBLOCK`, `conn_add_fdwatch`, and the on-error
  `psock_close` (caller owns the fd). Calls
  `conn_set_v3_owns_socket(c, 1)` before returning.
* **`server.h` forward-declares `t_connection`.**
* **`TcpSession::native_handle_int()`** -- new accessor.
* **`TcpSessionEgress`** in `tcp_bridge.cpp`: an
  `IConnectionEgress` adapter holding `weak_ptr<TcpSession>`.
* **`V3OwnedSession` bundle**: shared_ptr<TcpSession>,
  unique_ptr<TcpSessionEgress>, unique_ptr<LegacyBnetFrameRouter>,
  t_connection*, atomic torn_down flag.
* **`TcpBridgeImpl::sessions_`**: `mutex` + `vector<shared_ptr<
  V3OwnedSession>>` registry; callbacks capture `weak_ptr` so
  the registry is the single strong owner.
* **`spawn_v3_owned_session`**: builds the bundle, calls the
  legacy factory, wires conn -> router, starts router, hooks
  `on_bytes` -> `router.on_bytes` and `on_close` ->
  `teardown_session`, inserts in registry, calls
  `session.start()`.
* **`raw_handler` flip**: when `v3_tcp_session_mode != 0`,
  routes via `spawn_v3_owned_session` instead of releasing the
  fd to legacy. Default path byte-for-byte unchanged.

### Files changed

| Path | Nature |
| ---- | ------ |
| `src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/legacy_bnet_frame_router.hpp` | New `set_class_refresh` API |
| `src/v3/integration/legacy_bnetd/src/legacy_bnet_frame_router.cpp` | Implements + invokes class refresh after each successful dispatch |
| `src/v3/integration/legacy_bnetd/src/legacy_bnet_frame_router_link.cpp` | Dispatch-on-class; installs class-refresh hook |
| `src/bnetd/server.h` | Forward-decl `t_connection`; declare new factory |
| `src/bnetd/server.cpp` | Define `server_handle_v3_owned_bnet_socket` |
| `src/v3/integration/legacy_bnetd/src/tcp_bridge.cpp` | TcpSessionEgress + V3OwnedSession + spawn + raw_handler flag check |

### Build matrix after 38f

* `build/v3`: **694/694**.
* `build/legacy`: **698/698**.
* `build/linked`: **712/712**.

### Known runtime gaps documented at landing time

1. `udp_sock` shared per listener -- later audit (38g) concluded
   this is **non-issue**: legacy `sd_finalize_accepted` shares
   the same fd; `udp_sock` is only used for `psock_sendto` in
   `udptest_send.cpp` where the destination is per-conn but the
   source is per-listener; receive is registered once per listener.
2. **Thread safety of `conn_destroy` from Asio worker** -- racing
   the legacy main thread. Addressed in 38g.
3. Init-byte timing -- pending real-client verification.
4. Outqueue ordering with the redirect -- pending real-client
   verification.
5. `pvpgn` shutdown ordering -- addressed in 38g.

## 2026-06-21 (pp) -- Batch 38g: cross-thread teardown + shutdown ordering

User accepted runtime smoke-test responsibility for 38f and asked
the agent to land two of the documented gap items in one batch:

* **38g-thread**: post `teardown_session` to the legacy main loop
  instead of running it on the Asio worker thread that fired
  `on_close`.
* **38g-shutdown**: drain `sessions_` from `TcpBridge::stop()`
  before stopping the runtime.

### 38g-thread: main-loop post seam

* New seam in `src/bnetd/server.h`:
  `extern void server_post_to_main(std::function<void()> fn);`
* Implementation in `src/bnetd/server.cpp`: static
  `std::mutex pending_main_mu` guards a
  `std::vector<std::function<void()>> pending_main_q`. A
  `pending_main_active` flag is opened at the start of
  `_server_mainloop` and closed immediately after the loop exits;
  callbacks posted while the flag is closed are silently dropped
  (process is going away).
* `drain_pending_main()` swaps the queue under the mutex then
  invokes callbacks outside the mutex (so a callback that calls
  `server_post_to_main` will not deadlock). Wrapped in
  `try{}catch(...)` with an eventlog so a misbehaving callback
  cannot take down the loop.
* Hook point: right after `timerlist_check_timers(now)` and
  before the fdwatch poll, sharing the timer pass's
  single-threaded invariants.
* Added `<functional>`, `<mutex>`, `<vector>`, `<utility>`
  to `server.cpp`; `<functional>` to `server.h`.

### TcpBridge teardown refactor

`teardown_session` (in `tcp_bridge.cpp`) now splits work:

1. **Synchronous (any thread)**: remove the bundle's `shared_ptr`
   from `sessions_` under `sessions_mu_`.
2. **Posted to main loop**: `conn_set_v3_router(c, nullptr)`
   then `conn_destroy(c, nullptr, DESTROY_FROM_DEADLIST)`. The
   posted lambda captures the bundle's `shared_ptr` so the
   `V3OwnedSession` (and the `TcpSession` it owns) outlives
   `conn_destroy`. After the callback returns, the lambda
   destructs, dropping the last strong ref: the `TcpSession`
   destructs and the fd is closed.

The `torn_down` atomic guard still makes the flow idempotent.

### 38g-shutdown: drain in `stop()`

`TcpBridgeImpl::stop()` now, after closing the acceptors but
before stopping the runtime:

* Copies `sessions_` under `sessions_mu_`.
* Calls `session->close()` on each (outside the mutex so the
  on_close callback can re-enter `teardown_session`).
* Drops the copy.

The cooperative `runtime_->stop()` that follows joins worker
threads and runs queued handlers to completion. Legacy-side
`conn_destroy` posts queued during teardown either run on the
next main-loop iteration (if the loop is still running) or are
dropped (if the loop has exited, in which case `_shutdown_conns`
will reap the connection itself, skipping the fd close because
`v3_owns_socket = 1`).

### Files changed

| Path | Nature |
| ---- | ------ |
| `src/bnetd/server.h` | Declared `server_post_to_main`; `#include <functional>` |
| `src/bnetd/server.cpp` | Added queue + mutex + active flag + `drain_pending_main()`; hook in `_server_mainloop`; open/close gating around the `for(;;)` |
| `src/v3/integration/legacy_bnetd/src/tcp_bridge.cpp` | `teardown_session` posts conn_destroy via `server_post_to_main`; `stop()` drains `sessions_` |

### Build matrix after 38g

* `build/v3`: **694/694**.
* `build/legacy`: **698/698**.
* `build/linked`: **712/712**.

### Gap list status (carried over from 38f)

1. Cross-thread `conn_destroy` race -- **addressed** (38g-thread).
2. `stop()` shutdown ordering -- **addressed** (38g-shutdown).
3. `udp_sock` per listener -- **non-issue on audit** (legacy
   does the same; sendto-only usage; receive is per-listener).
   Closed without code action.
4. Init-byte timing -- still pending verification; depends on
   38g-thread being deployed (now done).
5. Outqueue ordering -- still pending verification; depends on
   real traffic.

### Next step

Operator runtime validation of the linked binary with
`v3_tcp_session_mode = 1`. Items 4-5 of the gap list need a real
client to drive them; pending that, the agent has no further
mechanical work to do on the TcpBridge.
