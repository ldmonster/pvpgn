

### 2026-05-12  Phase 0 kickoff
- Added `.editorconfig`, `.clang-format`, `.clang-tidy`.
- Added `CMakePresets.json` with `dev-debug`, `dev-release`, `dev-asan`, `ci-coverage` presets (all enabling `PVPGN_BUILD_V3`).
- Added root CMake option `PVPGN_BUILD_V3` (default OFF) — legacy build remains the default and is untouched.
- Created `src/v3/` sub-tree with its own `CMakeLists.txt` enforcing C++20 + warnings; pulls Catch2 v3 via `FetchContent` when `PVPGN_BUILD_TESTS=ON`.
- Implemented header-only `core/` library:
  - `result.hpp` — `Result<T,E>` / `Status` (no exceptions, no Boost dependency).
  - `error.hpp` — generic `StatusCode` enum + free helpers.
  - `strong_typedef.hpp` — opaque integer/string wrappers (`STRONG_TYPEDEF` macro + `StrongId<Tag,Underlying>`).
  - `bytes.hpp` — `ByteSpan`/`ByteView`, hex encoding/decoding.
  - `endian.hpp` — `read_le<T>` / `write_le<T>` / `read_be<T>` / `write_be<T>` with bounded checks.
  - `clock.hpp` — `IClock`, `SystemClock`, `ManualClock`.
  - `logging.hpp` — minimal `ILogger`, `NullLogger`, `ConsoleLogger`; spdlog adapter is a Phase-1 task.
  - `version.hpp` — semver constant + build metadata.
- Added Catch2 v3 unit tests covering all of the above (`tests/unit/core/`).
- Verified: `cmake --preset dev-debug && cmake --build --preset dev-debug && ctest --preset dev-debug` is green (see below).

### 2026-05-12  Phase 0 verified green
- Configured `build/v3` (v3-only, `PVPGN_BUILD_LEGACY=OFF`) with Ninja + GCC 13.3 + C++20.
- Built `pvpgn::v3::core` static library + 6 Catch2 test binaries.
- `ctest`: **25/25 tests pass** (Result/Status, endian round-trips + OutOfRange,
  ManualClock advance/set, StrongId hashing & comparison, hex round-trip, logger
  level filtering).
- Confirmed legacy build still builds bnetd/d2cs/d2dbs/bntrackd/bnpass + client tools
  with no changes (`build/legacy`, defaults).
- Fixes applied during integration:
  * `tests/unit/CMakeLists.txt` — `add_subdirectory(core)` (relative path).
  * Catch2 targets marked `SYSTEM TRUE` so strict warnings (`-Wnon-virtual-dtor`,
    `-Wold-style-cast`) don't fire on Catch2 internals.
  * `Result<T,E>` static_asserts `T != E` (variant constraint); test updated to
    use `Result<int, std::string>` for `map_error`.

### 2026-05-12  Phase 1 — logging, config, event-bus, scheduler
- Added FetchContent pins in `src/v3/CMakeLists.txt`:
  * **spdlog v1.14.1** (behind `PVPGN_V3_WITH_SPDLOG`, default ON).
  * **toml++ v3.4.0** (behind `PVPGN_V3_WITH_TOMLPP`, default ON).
  Both targets marked `SYSTEM` to keep our `-Werror` strict warnings clean.
- New header-only `core/` additions:
  * `event_bus.hpp` — type-keyed in-process pub/sub. RAII `Subscription`,
    `std::shared_mutex`-protected channel map, throw-safe publish.
  * `scheduler.hpp` — `IScheduler` interface + deterministic `ManualScheduler`
    (Asio-backed production impl is Phase 2).
  * `format.hpp` — `std::format`-style `LOG_TRACE..LOG_CRITICAL` macros that
    forward to `core::default_logger()`. Compile-time strip via
    `PVPGN_V3_LOG_LEVEL`.
- New `src/v3/infra/` modules:
  * `infra/log` (`SpdlogLogger`, `make_spdlog_logger`) — installs colour
    stdout sink + optional rotating file sink; bridges `core::ILogger`
    onto spdlog. Pattern `%Y-%m-%dT%H:%M:%S.%e %^%l%$ %v`.
  * `infra/config` — `ServerConfig`, `LogConfig`, `StorageConfig`,
    `NetworkConfig`; `parse_server_config()` / `load_server_config()` return
    `Result<ServerConfig, core::Error>` (no exceptions to callers).
    Recognised sections: `[server]`, `[network]`, `[log]`, `[storage]`.
- 18 new Catch2 test cases:
  * `tests/unit/core/event_bus_test.cpp` (5 cases — single/multi sub, type
    isolation, RAII unsubscribe, throwing-subscriber isolation).
  * `tests/unit/core/scheduler_test.cpp` (3 cases — fire on deadline, cancel,
    ordering of multiple timers).
  * `tests/unit/core/format_test.cpp` (2 cases — formatting macros + level
    filtering).
  * `tests/unit/infra/log/spdlog_logger_test.cpp` (3 cases — stdout sink,
    rotating file sink writes, runtime level changes).
  * `tests/unit/infra/config/server_config_test.cpp` (5 cases — defaults,
    full TOML round-trip, syntax error → `InvalidArgument`, missing file →
    `NotFound`, disk load).
- Verified: **43/43 tests pass** (`ctest --test-dir build/v3 --output-on-failure`).
- Legacy build still green and unchanged (no recompile needed).
- Issues fixed during integration:
  * `PVPGN_V3_LOG(level, ...)` macro renamed parameter to `lvl_` — the C
    preprocessor was substituting `level` inside `default_logger().level()`
    producing `LogLevel::Info()` garbage. Lesson recorded in repo memory.
  * toml++ `value_or<T>` takes `T&&`; rewrote call sites to pass rvalue
    defaults explicitly (`std::size_t{cfg.log.rotate_size}`) so GCC 13
    doesn't reject lvalue-to-rvalue-ref bindings.

### 2026-05-12  Phase 4 kick-off — protocol common layer
- New header-only library `protocol_common` under `src/v3/protocol/common/`:
  * `packet.hpp` — `BnetHeader` (marker/code/size, 4 bytes LE),
    `parse_bnet_header()`, `write_bnet_header()`, `parse_packet()`
    (returns `{Packet, consumed}` or `OutOfRange` when incomplete so the
    caller can wait for more bytes).
  * `reader.hpp` — `Reader` over a `core::ByteView`; bounded
    `read_le<T>`, `read_be<T>`, `read_bytes`, `read_cstring`, `skip`.
    On OOB returns `core::Error{OutOfRange}` **without** advancing the
    cursor (replay/fuzz-friendly).
  * `writer.hpp` — growing `std::vector<std::byte>` buffer with
    `write_u8/le/be/bytes/cstring`, `begin_bnet_packet(code)` +
    `finalize_bnet_packet()` to back-patch the 16-bit size field.
- Wired as `INTERFACE` library in `src/v3/CMakeLists.txt`
  (`pvpgn_v3_add_library(protocol_common INTERFACE ...)`).
- 18 new Catch2 cases in `tests/unit/protocol/common/` covering:
  header parse/write round-trip, marker/size validation, partial-buffer
  framing, LE/BE int reads, NUL-string parsing, OOB safety,
  Writer round-trip via parse_packet → Reader, `take()` semantics.
- Issues fixed:
  * `short_()` helper originally returned `Result<ByteView>` which
    couldn't convert into `Result<uint16_t>` etc. Changed it to return
    `core::Failure<core::Error>` so the implicit conversion to any
    `Result<T,Error>` kicks in.
  * Catch2 fails to link `StringMaker<std::string_view>` symbols in
    its v3.5.4 amalgamated build; rewrote test comparisons to
    `std::string{view} == "..."`. Added to repo memory.
- Verified: **61/61 tests pass** (`ctest --test-dir build/v3`).

### 2026-05-12  Phase 4 — first bnet codec (SID_NULL / SID_PING / SID_AUTH_INFO)
- New static library `protocol_bnet` under `src/v3/protocol/bnet/`:
  * `messages.hpp` — value-type messages (`Null`, `Ping`, `AuthInfo`)
    and `ClientMessage` / `ServerMessage` variants. No domain types,
    no I/O.
  * `codec.hpp` / `codec.cpp` — pure `decode_client(Packet)` /
    `decode_server(Packet)` returning `Result<Variant>` and overloaded
    free `encode(Writer&, const Msg&)` returning `Status<>`. Unknown
    SID codes → `Unimplemented`; short/malformed payloads → the
    underlying `Reader` error (`OutOfRange` / `InvalidArgument`).
- Wire-format parity with legacy `bnet_protocol.h`:
  * SID_NULL  → `FF 00 04 00`
  * SID_PING  → `FF 25 08 00 <ticks LE>`
  * SID_AUTH_INFO → header + 9 × u32 LE + 2 × NUL-string
- 10 new Catch2 cases under `tests/unit/protocol/bnet/`:
  * Round-trip via real wire bytes (`encode` → `parse_packet` →
    `decode_client`) for `Null`, `Ping` (both client + server),
    `AuthInfo` (with realistic `IX86`/`SEXP` tag values).
  * Byte-exact wire snapshots (`FF 00 04 00`, `FF 25 08 00 …`).
  * Negative cases: unknown SID code, SID_NULL with extra body,
    SID_PING with short body, SID_AUTH_INFO with unterminated string.
- Verified: **71/71 tests pass** (`ctest --test-dir build/v3`).
- Legacy build unaffected (no changes outside `src/v3/`).

### 2026-05-12  Phase 1 close-out + Phase 2 network spine
- **Phase 1 finish (additive, no legacy touches)**
  * `infra/config/legacy_prefs.hpp` — `LegacyPrefs` value type that mirrors
    the legacy `prefs_get_*` accessor surface (`servername()`, `bind_addr()`,
    `port()`, `script_dir()`, `storage_*()`, `log_*()`) on top of a typed
    `ServerConfig`. Cached `std::string` views for `path` fields keep the
    accessors `string_view`-clean on Linux *and* Windows. `make_legacy_prefs()`
    returns a `shared_ptr` snapshot suitable for atomic hot-reload swap.
  * `protocol/common/replay.hpp` — generic
    `replay<Decoded>(ByteView, DecodeFn) -> Result<ReplayResult<Decoded>>`.
    Iterates `parse_packet`, decodes each frame via the supplied codec, and
    surfaces partial-tail bytes through `stats.bytes_trailing` (any other
    decode error propagates). Designed for golden-tests, shadow-mode parity
    checks, and libFuzzer entry points.
  * Deferred to per-module migrations (per plan §15.1 "no big bang"):
    `IClock` routing into legacy globals, `eventlog→LOG_*` sweep,
    `xalloc/xstr/scoped_ptr` mass migration. Legacy code remains untouched.
- **Phase 2 — Asio network spine**
  * `src/v3/CMakeLists.txt` — new `PVPGN_V3_WITH_BOOST` option (default ON) +
    `PVPGN_V3_WITH_FIBER` option (default OFF). Calls
    `find_package(Boost 1.75 REQUIRED COMPONENTS system [fiber context])`;
    on Ubuntu 24.04 picks up the system Boost 1.83 packages.
  * New static library **`infra_net`** under `src/v3/infra/net/`:
    - `io_runtime.hpp/.cpp` — owns one `asio::io_context`, an
      `executor_work_guard`, a worker thread pool, and an `asio::signal_set`.
      `run(threads)` spawns workers; `stop()` cancels signals, drops the work
      guard, stops the context, and joins workers. `install_signal_handlers`
      wires graceful shutdown on SIGINT/SIGTERM.
    - `tcp_session.hpp/.cpp` — `shared_from_this` session that owns a
      `tcp::socket` plus an `asio::strand`. Read loop calls
      `async_read_some` into a 4 KiB scratch and forwards via `on_bytes`.
      Write path is a strand-serialised deque with at most one in-flight
      `async_write`. `close()` is idempotent and triggers `on_close`.
    - `tcp_acceptor.hpp/.cpp` — opens/binds/listens (incl. `SO_REUSEADDR`),
      then loops `async_accept` and hands accepted sockets to a
      `SessionFactory`. Two `listen()` overloads (raw endpoint vs.
      `host:port` string); both return the bound endpoint so port-0 tests
      can discover the assigned port. Errors map to `core::StatusCode`.
    - `fiber.hpp` — header-only optional helper compiled only when
      `PVPGN_V3_WITH_FIBER=ON`: `spawn_on(IoRuntime&, F)`, `yield()`,
      `sleep_for()` re-exports.
  * New tests `tests/unit/infra/net/echo_test.cpp` (3 cases): end-to-end
    loopback echo with a real synchronous Asio client; `IoRuntime::post`
    executes off-thread; invalid bind address → `InvalidArgument`.
  * New tests `tests/unit/infra/config/legacy_prefs_test.cpp` (1 case):
    full surface coverage of the prefs adapter.
  * New tests `tests/unit/protocol/common/replay_test.cpp` (4 cases):
    full-stream round-trip, partial tail surfaced via stats, hard error
    propagation, empty input.
- **Fixes during integration**
  * `Result<T,E>` cannot implicit-construct from `Error`; all
    `tcp_acceptor.cpp` error returns now wrap with `core::fail(...)`.
    Lesson reinforced from earlier session.
  * `IoRuntime` initially tried to rebuild its `executor_work_guard` after
    `stop()`; the type isn't copy/move-assignable, so the runtime is now
    explicitly one-shot (construct fresh for a new lifecycle).
  * Boost.Asio's `any_executor::equal_ex` trips `-Wnull-dereference` on
    GCC 13. The guard is correct but invisible to the analyser; relaxed
    PUBLICly on `infra_net` so consumers inherit the suppression.
  * Shadowed lambda capture (`auto s = s_wk.lock()` shadowing the outer
    `s` factory parameter) renamed to `sp`.
- Verified: **79/79 tests pass** (`ctest --test-dir build/v3 --output-on-failure`).
  Legacy build still `ninja: no work to do.`

### 2026-05-12  Phase 3 kick-off — shared value objects + Account aggregate
- New header-only **`domain_shared`** INTERFACE library under
  `src/v3/domain/shared/`:
  * `ids.hpp` — `AccountId`, `ChannelId`, `GameId`, `ClanId`, `TeamId`
    via `core::StrongId<Tag,u32>` (cross-aggregate refs are *always* IDs,
    never pointer aliases per plan §3.1).
  * `client_tag.hpp` — `ClientTag` validates 4-byte printable ASCII.
    Stored in human-readable order; `packed_be()` matches the BNet
    wire byte order (`STAR` → `0x53544152`).
  * `user_name.hpp` — `UserName` enforces the legacy `account_check_name`
    rule (2..15, `[a-zA-Z0-9_.\-]`, leading letter) at construction.
    Two-string storage: `display_` preserves casing, `canonical_` is
    lower-case for `operator==` / hashing.
  * `locale.hpp` — `Locale::parse_or_default()` returns `enUS` for
    garbage input; domain code never throws.
  * `bn_hash.hpp` — 20-byte Broken-SHA-1 output. `equals_constant_time()`
    is the only equality operator — login is attacker-facing.
  * `ip_address.hpp` — Variant of `array<u8,4>` / `array<u8,16>`.
    Pure parser for dotted-quad + full-form IPv6 (no `::` compression —
    keeps the parser tiny). DNS lives in `infra/net`, not here.
  * `ban.hpp` — `Ban{scope, reason, issuer, issued_at, expires_at}`
    plus `active_at(SystemTime)` predicate. Stays a value type so the
    Account aggregate can own it directly.
  * `events.hpp` — `DomainEvent` variant. First eight identity events
    land here: `AccountCreated`, `UserLoggedIn`, `UserLoginRejected`
    (with `Reason` enum), `UserLoggedOut`, `AccountPasswordChanged`,
    `AccountCommandGroupGranted`, `AccountBanned`, `AccountUnbanned`.
- New header-only **`domain_identity`** INTERFACE library under
  `src/v3/domain/identity/`:
  * `account.hpp` — `Account` aggregate.
    - `create(id, name, hash, locale)` factory returning `Result<Account>`
      + emitting `AccountCreated`.
    - `rehydrate(...)` repository constructor that emits **no** events.
    - `login(candidate_hash, ip, tag, now)` — checks `locked_`, then
      `ban_.active_at(now)`, then constant-time hash compare; emits
      exactly one of `UserLoggedIn` / `UserLoginRejected`; auto-clears
      expired bans (emitting `AccountUnbanned`).
    - `change_password`, `apply_ban`, `clear_ban`, `grant_command_group`
      (idempotent — no duplicate event when already granted),
      `revoke_command_group`, `lock`, `unlock`.
    - `CommandGroupMask` (8-bit `std::bitset`) with `is_admin()` set
      iff group 7 or 8 is granted — preserves legacy semantics.
    - `drain_events()` moves the pending event buffer out for the
      Application layer to publish.
- CMake: two new `pvpgn_v3_add_library` blocks (`domain_shared`,
  `domain_identity`); both INTERFACE; depend on `core`.
- 18 new Catch2 cases:
  * `tests/unit/domain/shared/value_objects_test.cpp` (9 cases —
    `ClientTag`, `UserName` validity / case-insensitive equality,
    `Locale` fallback, `BNHash` size + constant-time equality, `IpAddress`
    v4 + v6 happy / sad).
  * `tests/unit/domain/identity/account_test.cpp` (9 cases — `create`
    emits `AccountCreated`; successful login emits `UserLoggedIn`;
    wrong hash → `InvalidCredentials`; active ban blocks even with
    correct hash; expired ban auto-clears and login proceeds (two
    events: `AccountUnbanned` then `UserLoggedIn`); lock blocks login;
    `grant_command_group` idempotent + admin detection;
    `change_password` lets login succeed with new hash; `rehydrate`
    is silent).
- **Fixes during integration**
  * `core::SystemTime` was a class-scope alias inside `IClock`, not
    visible at namespace scope. Domain code wants the bare name;
    hoisted to `pvpgn::core::SystemTime` / `MonotonicTime`. The
    `IClock` aliases now forward to the namespace-scope names.
    GCC's "error recovery" silently replaced the unknown type with
    `int` — the diagnostics blamed test callers; lesson recorded.
  * `std::bitset::set/reset/test/any` are not `constexpr` until C++23.
    Removed `constexpr` from `CommandGroupMask` mutators/queries while
    keeping the default constructor `constexpr`.
  * `-Wmaybe-uninitialized` fired inside libstdc++'s `std::variant`
    move-construction visitor when the variant held alternatives with
    non-trivial members (`Ban` with `std::string reason`, `BanScope`
    enum). This is a long-standing GCC false positive in `<variant>`.
    Added `-Wno-maybe-uninitialized` to the v3 default warning set in
    `cmake/v3.cmake` with a comment explaining why.
- Verified: **97/97 tests pass** (`ctest --test-dir build/v3
  --output-on-failure`). Legacy build still `ninja: no work to do.`

### 2026-05-12 (cont.)  Phase 3 — five aggregates land, 26 events
- New header-only INTERFACE libraries under `src/v3/domain/`:
  * **`domain_chat`** — `chat::Channel` aggregate. `ChannelFlags`
    bitset collapses the legacy `channel_flags_*` enum into a single
    `std::bitset<8>` (Public/Permanent/Moderated/Restricted/Silent/
    System/AllowBots/Locked). `ChannelPolicy { flags, max_members,
    client }` is the construction-time invariant set. Members are an
    `AccountId`-keyed `std::unordered_map` — no `t_connection*` aliases.
    `admit/leave/post/kick/set_topic` emit
    `ChannelJoined/Left/MessageSent/MemberKicked/TopicChanged`
    or `ChannelJoinRejected{Full|Banned|WrongClientTag|Locked}`.
    `admit` is idempotent; `kick` adds the target to the banlist
    (legacy behaviour preserved).
  * **`domain_social`** — `FriendList` (25-cap, self-rejection,
    idempotent add) + `Clan` aggregate. `Clan::create` validates 2..4
    printable-ASCII tag and 1..25 name, seats the founder as
    `Chieftain`, and emits both `ClanCreated` and `ClanMemberJoined`.
    Ranks: Chieftain/Shaman/Grunt/Peon; 250-member cap from legacy.
  * **`domain_gameplay`** — `Game` aggregate with deterministic FSM
    `Open → InProgress → Reporting → Finalized`. `host(...)` validates
    descriptor and seats host as the first player; `start` is
    host-only; `finalize(results, now)` emits `GameEnded` carrying a
    full `MatchReport` consumable by the ladder service. Wall-clock is
    always caller-supplied — no `std::chrono::system_clock::now()`.
  * **`domain_ladder`** — `LadderCalculator` pure stateless service.
    Elo with a configurable `LadderRules { k_factor,
    disconnect_is_loss }` per client-tag (defaults match W3 K=32; SC
    typically passes K=16). Each player's expected score is computed
    against the *opponent* mean (self excluded), so 1v1 underdogs
    gain more rating than favourites. `disconnect → loss` is the
    default (matches legacy `ladder_calc.cpp`).
  * **`domain_moderation`** — `IpBanList` aggregate. `add` is
    idempotent on duplicate IP (replaces older entry, emits
    `IpBanAdded`). `blocks(ip, now)` honours `expires_at`.
    `prune_expired(now)` drops stale rows and emits `IpBanRemoved`
    per drop. CIDR ranges deferred to a follow-up commit.
- New shared value objects (consumed by `events::DomainEvent`):
  * `domain/shared/chat_message.hpp` — `ChatMessage` bounded
    (1..223 bytes, no `\n\r\0`) via `create()` returning
    `Result<ChatMessage>`.
  * `domain/shared/match_report.hpp` — `MatchOutcome` enum
    (Win/Loss/Draw/Disconnect), `PlayerResult`, `MatchReport
    { game, client, results, finished_at }`.
- `domain/shared/events.hpp` grew from **8** to **26** alternatives:
  added `ChannelJoined`, `ChannelLeft`, `ChannelJoinRejected` (with
  `Reason` enum), `ChannelMessageSent`, `ChannelTopicChanged`,
  `ChannelMemberKicked`, `FriendAdded`, `FriendRemoved`,
  `ClanCreated`, `ClanMemberJoined`, `ClanMemberLeft`, `GameCreated`,
  `GameStarted`, `GamePlayerJoined`, `GamePlayerLeft`, `GameEnded`,
  `IpBanAdded`, `IpBanRemoved`.
- CMake: five new `pvpgn_v3_add_library` blocks (all INTERFACE,
  depending on `core` + `domain_shared`).
- 26 new Catch2 cases across `tests/unit/domain/{chat,social,gameplay,
  ladder,moderation}/` covering FSM transitions, idempotency,
  capacity gates, validation rejections, ban gating, event-payload
  shape, and Elo math.
- **Fixes during integration**
  * `Clan::find_(AccountId)` declared with `auto` deduction was used
    by `contains()` before the body was visible — GCC rejected
    ("use of 'auto …' before deduction"). Split into
    `find_mut_`/`find_const_` with explicit `iterator` return types.
  * Initial `LadderCalculator` averaged across **all** entries,
    making 1v1 self-only `entries` slices produce identical rating
    deltas regardless of input. Switched to a per-player
    `opponent_mean_` that excludes `self`; underdog/favourite
    asymmetry now holds.
- Verified: **123/123 tests pass** (`ctest --test-dir build/v3
  --output-on-failure`). Legacy build still untouched.

### 2026-05-12 (cont. 2)  Phase 3 — remaining aggregates land, 142/142
- New header-only INTERFACE libraries:
  * **`domain_matchmaking`** — `AnonGameQueue` (FIFO with idempotent
    enqueue, `can_match()` predicate, `match(GameId)` pulls
    `2*team_size` oldest entries and emits `AnonGameMatched`) and
    `Tournament` (single-elim scheduler, ≥2 participants, emits
    `TournamentScheduled`).
  * **`domain_realm`** — `Realm` aggregate carrying a vector of
    `Character` values; 16-char name limit, case-insensitive
    uniqueness, one-way `unregister` emitting `RealmUnregistered`.
    Replaces legacy `bnetd/realm.cpp` + `d2cs/d2charfile.cpp`
    invariants (storage adapters land in Phase 5).
- New aggregates added to existing libraries:
  * `social::Team` (`domain_social`) — fixed roster of 2..4 unique
    members for W3 AT ladder. Immutable after creation; `disband()`
    is one-way and idempotent.
  * `moderation::Quota` (`domain_moderation`) — sliding-window rate
    limiter. `record(now)` returns `Allowed`/`Throttled`/`Muted`;
    once limit is exceeded the account is muted for `policy.mute_for`
    and the limiter emits `AccountQuotaExceeded`. Mute auto-lifts.
  * `identity::AttributeMap` (`domain_identity`) — typed wrapper over
    the legacy `BNET\acct\*` stringly-typed bag. Idempotent on
    same-value writes; emits `AccountAttributeChanged` only on
    real changes; `rehydrate()` is silent.
- `IpBanList` extended with CIDR ranges:
  * `add_range(network, prefix_bits, ...)`, `remove_range`,
    `range_count()`.
  * `blocks(ip, now)` now also walks the range table with a
    bit-prefix comparator that handles both v4 and v6 (no `::`
    compression needed — we already store full octets/groups).
  * `prune_expired()` covers ranges too.
- `events::DomainEvent` variant grew from **26** to **40**
  alternatives. New events: `IpBanRangeAdded`, `IpBanRangeRemoved`,
  `AccountQuotaExceeded`, `TeamCreated`, `TeamDisbanded`,
  `AnonGameQueued`, `AnonGameDequeued`, `AnonGameMatched`,
  `TournamentScheduled`, `RealmRegistered`, `RealmUnregistered`,
  `CharacterCreated`, `CharacterDeleted`, `AccountAttributeChanged`.
- CMake: three new INTERFACE libraries (`domain_matchmaking`,
  `domain_realm`; `domain_social` / `domain_moderation` /
  `domain_identity` gained new headers without new targets).
- 19 new Catch2 cases across
  `tests/unit/domain/{social,moderation,matchmaking,realm,identity}/`:
  Team validation + disband, IpBanList CIDR /24+/32+expiry+prune,
  Quota under-limit/exceed/auto-lift, AnonGameQueue idempotency
  +match+dequeue, Tournament scheduling, Realm character lifecycle,
  AttributeMap set/get/erase/rehydrate.
- **Fix**: Catch2 `StringMaker<std::string_view>` is still missing
  in the amalgamated 3.5.4 build (already noted in repo memory).
  Replaced raw `view == "literal"` comparisons in
  `attribute_map_test.cpp` with `std::string{view} == "literal"`.
- Verified: **142/142 tests pass** (`ctest --test-dir build/v3
  --output-on-failure`). Legacy build still untouched.

### 2026-05-12 (cont. 3)  Phase 4 — protocol decoupling progresses
- BNet codec extended from 3 SIDs to 9 (10 wire codes — LOGONRESPONSE2
  shares 0x3A for both directions). New messages all carry strict
  round-trip tests:
  * `SID_LOGONRESPONSE2` (0x3A) — client (`LogonResponse2` with 5×u32
    SHA-1 hash) and server reply (`LogonResponse2Reply`, tolerant of
    optional reason string).
  * `SID_AUTH_CHECK` (0x51) reply (`AuthCheckReply`).
  * `SID_JOINCHANNEL` (0x0C), `SID_ENTERCHAT` (0x0A) both directions,
    `SID_CHATCOMMAND` (0x0E), `SID_CHATEVENT` (0x0F).
- New header-only-with-impl library **`protocol_irc`** (`src/v3/protocol/irc/`):
  * `try_parse_line(buf)` — streaming framer; finds first CRLF (or
    bare LF, for legacy clients) and returns `{line, consumed}`. Pure;
    `OutOfRange` means "wait for more bytes".
  * `decode(line)` — tokenises into `irc::Message{prefix, command,
    params}`. Trailing param prefixed with ':' captures spaces;
    commands are upper-cased ASCII for case-folded compare.
  * `encode(msg)` / `encode_to_string(msg)` — emits CRLF-terminated
    wire bytes; auto-marks the last param as trailing when it contains
    spaces, starts with ':', or is empty.
- CMake: added `protocol_irc` STATIC library wired into `src/v3/`;
  `tests/unit/protocol/irc/` registered under `tests/unit/protocol/`.
- 18 new Catch2 cases:
  * BNet: 8 cases — LOGONRESPONSE2 client/server (with + without
    reason), JOINCHANNEL, ENTERCHAT both, CHATCOMMAND, CHATEVENT,
    AUTH_CHECK reply.
  * IRC: 10 cases — CRLF framing, bare-LF tolerance, `OutOfRange`
    when truncated, PRIVMSG with prefix+trailing, lowercase commands
    folded, empty/prefix-only rejected, round-trip encode/decode,
    trailing-marker rules for ':'-prefixed/empty last params,
    empty-command rejection.
- Verified: **160/160 tests pass** (was 142/142 after Phase 3).

### 2026-05-12 (cont. 4)  Phase 4 — per-protocol FSM skeletons land
- **`protocol::bnet::BnetFsm`** + **`ISessionContext`** abstraction.
  States `Init → AuthInfoReceived → LoggedIn → InChat → Closing`.
  `handle(ClientMessage)` dispatches via `std::visit`; out-of-order
  packets transition to `Closing` and return `InvalidArgument`. The
  FSM only orchestrates the wire dance — real domain mutations
  (version-check, account lookup, channel join) are deferred to
  Phase 5 (the seams are explicit in the source as `Phase-5 hooks`).
- **`protocol::irc::IrcFsm`** + **`ISessionContext`** with
  `server_name()`. States `Greeting → Registered → InChannel →
  Closing`. Emits the canonical numerics
  001/421/431/451/461 and answers PING with PONG; JOIN echoes
  membership with the user's prefix and follows up with 366
  RPL_ENDOFNAMES. QUIT closes the session.
- CMake: added `protocol/bnet/src/fsm.cpp` and
  `protocol/irc/src/fsm.cpp` to their respective static libs. No new
  public targets — both FSMs ship inside `protocol_bnet` /
  `protocol_irc`.
- 15 new Catch2 cases covering both FSMs with a `FakeContext` that
  captures outbound messages:
  * BNet: PING mirror in Init; AUTH_INFO transitions + reply; AUTH_INFO
    out-of-order closes; full happy path Init→InChat; empty-username
    LOGONRESPONSE2 fails with result 0x01 and no transition;
    JOINCHANNEL before ENTERCHAT closes.
  * IRC: NICK alone is silent; NICK+USER triggers 001 RPL_WELCOME;
    NICK with no nickname → 431; USER with too few params → 461;
    PING → PONG mirroring cookie; PRIVMSG before registration → 451;
    JOIN echoes + 366; unknown command → 421; QUIT closes.
- **Test count: 175/175 pass** (was 160/160).
- Catch2 string_view link-bug bit us once more in the IRC FSM test —
  wrapped `f.channel() == "#pvpgn"` in `std::string{...}`.

### 2026-05-12 (cont. 5)  Phase 4 — UDP + telnet + file + d2cs codecs
- Four new `STATIC` libraries under `src/v3/protocol/`:
  * **`protocol_udp`** — connection-less. Reads first u32 type then a
    type-specific tail; `Datagram = variant<UdpTest, UdpPing,
    SessionAddr1, SessionAddr2>`. No FSM needed.
  * **`protocol_telnet`** — admin line protocol. `try_parse_line()`
    streaming framer (CRLF or bare LF), `tokenise()` whitespace
    splitter into `Command{verb, args}`, `write_line()` reply helper.
  * **`protocol_file`** — BNFTP. `FileHeader{u16 size, u16 type}` plus
    `ClientFileReq` (0x0100) and `ServerFileReply` (0x0000) with
    arch/client tags, ad/extension ids, 64-bit Windows timestamp, and
    NUL-terminated filename.
  * **`protocol_d2cs`** — 3-byte header (`u16 size + u8 type`).
    Implements the D2CS login round-trip (LoginReq 0x01 with
    11×u32 fields + 5×u32 secret hash + cstring account name;
    LoginReply 0x01 with u32 reply code, `kLoginReplyOk` /
    `kLoginReplyBadPass`). Create-char / create-game / join-game
    deferred to the Phase-5 realm wiring step.
- All four codecs follow the established pattern: pure header-level
  `decode()` returning `Result<Variant>` with `OutOfRange` /
  `Unimplemented` errors, plus per-message `encode(Writer&, …)`.
- 17 new Catch2 cases:
  * UDP: 6 — round-trip for all four datagram types + unknown-type
    and short-buffer rejection.
  * Telnet: 5 — CRLF split, bare-LF tolerance, multi-arg tokenise,
    empty-line yields empty verb, `write_line` appends CRLF.
  * File: 3 — `ClientFileReq` and `ServerFileReply` round-trip, header
    rejects size < kSize.
  * D2CS: 3 — LoginReq + LoginReply round-trip, header rejects size <
    kSize.
- **Test count: 192/192 pass** (was 175/175).

### 2026-05-12 (cont. 6)  Phase 4 — D2GS bridge codec
- `protocol_d2gs` static library. Header `D2gsHeader{u16 size, u16 type,
  u32 seqno; kSize=8}`. Two direction-tagged variants —
  `DownMessage = variant<SetGsInfo, EchoReq, Control>` for the D2CS → D2GS
  direction and `UpMessage = variant<SetGsInfo, EchoReply>` for the
  reverse — disambiguate the 0x13 echo half-duplex and the upstream-only
  rejection of 0x14 control.
- Messages implemented: `SETGSINFO` (0x12, maxgame + gameflag, both
  directions), `ECHOREQ` / `ECHOREPLY` (0x13, empty body), `CONTROL`
  (0x14, cmd + value with `kControlRestart` / `kControlShutdown`).
  `AUTHREQ` / `AUTHREPLY` (0x10 / 0x11) deferred — those use direction-
  dependent semantics on the same code that the Phase-5 router will tag.
- 5 new Catch2 cases covering both round-trip paths, the direction-aware
  control rejection on upstream, and short-header rejection.
- **Test count: 197/197 pass** (+5 from previous 192).

### 2026-05-12 (cont. 7)  Phase 4 — Westwood Online gameres codec
- New `protocol_wolgameres` static library. The legacy `bn_int_nget` /
  `bn_short_nget` macros mean **every multi-byte int is big-endian**;
  the v3 `Reader` / `Writer` already expose `read_be<T>()` /
  `write_be<T>()` so the codec is straightforward TLV walking.
- Wire format implemented in full:
  * `Header{u16 size, u16 rngd_size; kSize=4}`.
  * Optional 4-byte zero prefix (legacy "RNDG marker") detected and
    surfaced as `Report::has_rndg_prefix`.
  * Repeated TLVs: `u32 tag` (FourCC) + `u16 data_type` + `u16 data_len`
    + raw payload bytes. `DataType` enum exposes the legacy
    `kByte/kBool/kTime/kInt/kString/kBigInt` constants.
- Tag semantics deliberately live above this layer — callers receive
  `Report{header, has_rndg_prefix, entries}` and pick the meaning per
  tag. Three convenience accessors decode common entry payloads:
  `read_byte`, `read_int` (BE 32-bit), `read_bigint` (BE 64-bit), and
  `read_string` (trailing-NUL trimmed view).
- 4 new Catch2 cases: empty report round-trip, full round-trip with
  three mixed-type entries (SER#/IDNO/FINI) + RNDG prefix, unknown
  data-type rejection, short-header rejection.
- New repo-memory note: GCC 13 `-Warray-bounds` false positive when
  copying a `vector<byte>` of size 1 — suppress with a local pragma
  in affected TUs.
- **Test count: 201/201 pass** (+4 from previous 197).

### 2026-05-12 (cont. 8)  Phase 4 — D2CS realm messages + D2GS auth
- `protocol_d2cs`: client variant grew `LoginReq → +CreateCharReq +CreateGameReq +JoinGameReq`; server variant grew `LoginReply → +CreateCharReply +CreateGameReply +JoinGameReply`.
  * `decode_client()` / `decode_server()` rewritten as `switch (hdr.type)` dispatchers (replacing the old single-message helpers); `body_for()` retired in favour of a direction-agnostic `body_of()` that returns both header and body view.
  * Status-code constants imported verbatim from `src/common/d2cs_protocol.h`: `kCreateCharReply{Ok,Failed,AlreadyExists,NameRejected}`, `kCreateGameReply{Ok,Failed,InvalidName,NameExists,ServerDown,Unavailable}`, `kJoinGameReply{Ok,Failed,BadPass,NotFound,Full,Level}`.
- `protocol_d2gs`: added the AUTHREQ/AUTHREPLY family with **direction-tagged** structs that disambiguate the shared 0x11 wire code:
  * `DownAuthReq` (D2CS → D2GS, type 0x10) — `u32 session_num`, `u32 signlen`, cstring realm, raw key checksum.
  * `UpAuthReply` (D2GS → D2CS, type 0x11) — `u32 version/checksum/randnum/signlen` + 128-byte sign block.
  * `DownAuthReply` (D2CS → D2GS, type 0x11) — `u32 reply` with `kAuthReply{Ok,BadVersion,BadChecksum}`.
  * `DownMessage` and `UpMessage` variants extended accordingly. The direction split is what makes the 0x11 collision unambiguous; the new `0x11 disambiguated by direction` test pins this behaviour.
- 10 new Catch2 cases. Header-pattern note: `Writer::write_cstring` now guards `memcpy` against empty strings — GCC 13 was tripping `-Wstringop-overflow=` on `memcpy(p, data, 0)` when the encoder fed an empty pass-phrase.
- **Test count: 211/211 pass** (+10 from previous 201).

## 2026-05-12 (cont. 9) — Phase 5 begins: Application layer

Bootstrapped the application layer per refactoring-plan-04. New tree
`src/v3/application/{ports,auth}` plus a new in-memory adapter
collection in `src/v3/infra/inmemory/`. First use-case end-to-end:
`LoginUser`.

New ports (interface-only, header-only, namespace `pvpgn::application::ports`):
- `application/ports/event_bus.hpp` — `IEventBus` with subscribe/
  unsubscribe + handler isolation contract.
- `application/ports/account_repository.hpp` — `IAccountRepository`
  with `find_by_id`, `find_by_name`, `save`, `remove`, `size`. All
  returns are `core::Result<T>` / `core::Status<>`.
- `application/ports/session_registry.hpp` — `ISessionRegistry`
  enforcing the single-session-per-account policy through
  `attach`/`detach`/`session_for`/`account_for`/`list`.

New in-memory adapters (`pvpgn::infra::inmemory`):
- `event_bus.hpp` — snapshot-and-iterate; per-handler `try/catch`.
- `account_repository.hpp` — `unique_ptr<Account>` keyed by id, with
  a secondary canonical-name index.
- `session_registry.hpp` — bidirectional `unordered_map` pair.

Domain additions:
- `domain/shared/ids.hpp` gains `SessionId = StrongId<SessionIdTag, u64>`.

Application use-case:
- `application/auth/include/application/auth/login_user.hpp` +
  `src/login_user.cpp`. `execute()` orchestrates lookup → aggregate
  authentication → event drain → session attach → persistence. Maps
  `Account::LoginOutcome` to a typed `LoginError`.

Tests: `tests/unit/application/auth/login_user_test.cpp` covers happy
path, unknown user, bad password, duplicate session, locked account.

Build/test gates: green. ctest reports **216/216** (was 211 — +5).

## 2026-05-12 (cont. 10) — infra_net: UdpEndpoint

Adds the connectionless counterpart to `TcpSession`/`TcpAcceptor`,
needed by the BNet UDP tracking probe (`handle_udp_packet`) and the
future admin/telnet datagram channel.

- `src/v3/infra/net/include/infra/net/udp_endpoint.hpp` +
  `src/udp_endpoint.cpp`. Strand-serialised receive loop. Outbound
  queue with single in-flight `async_send_to`. `bind()` returns
  `core::Result<udp::endpoint>` exposing the OS-picked port for
  ephemeral binds. `OnDatagram(remote, ByteView)` / `OnError`
  callbacks; `send_to()` is fire-and-forget and thread-safe.
- `tests/unit/infra/net/udp_echo_test.cpp` — loopback echo round-trip
  + `InvalidArgument` parse-failure path.

Tests: **218/218** (was 216 — +2).

## 2026-05-12 (cont. 11) — Phase 2 seam: LegacyProtocolHandler (framing-only)

Lays the strangler-fig seam called for in
refactoring-plan-15 §"Network spine on Asio + Fiber" item 2,
**without yet linking legacy bnetd**. The seam crystallises the
contract; a follow-up step refactors `src/bnetd/CMakeLists.txt`
into a `bnetd_legacy` static library and a thin executable, then
overrides `dispatch_frame()` to call the real `handle_*_packet`.

New ports:
- `application/ports/connection_handler.hpp` — `IConnectionHandler`
  + `IConnectionEgress` + `ConnectionHandlerFactory`. The seam
  through which `infra::net::TcpSession` will hand frames to
  protocol code (legacy or v3-native).

New adapter (`pvpgn::integration::legacy_bnetd`):
- `legacy_protocol_handler.hpp/.cpp` — per-session stateful
  framing for the eleven legacy connection classes:
  - **Init**: 1 byte (the `CLIENT_INITCONN_CLASS_*` magic).
  - **Bnet**: u16 type LE + u16 size LE (size = total frame size).
  - **File / D2csBnetd / W3route**: u16 LE size at offset 0.
  - **WolGameres**: u16 BE size at offset 0.
  - **Bot / Telnet / Irc / Wol / Wladder**: line-terminated; flushes
    at `MAX_PACKET_SIZE` overflow per legacy behaviour.
  Malformed declared sizes flush the buffer to resync. `set_class()`
  flips the class after the Init magic byte is consumed. The hook
  `dispatch_frame(LegacyFrame)` is virtual; the seam-only build
  accumulates frames into a vector for inspection.

New CMake target: `integration_legacy_bnetd` (STATIC, depends on
`core` + `application_ports` only — no legacy linkage).

Tests: `tests/unit/integration/legacy_bnetd/legacy_protocol_handler_test.cpp`
covers per-class framing, partial-read stitching, malformed-size
recovery, line-mode trailing-fragment buffering, mid-stream class
switch, and post-close inertness.

Tests: **226/226** (was 218 — +8).

Outstanding from the user's roadmap:
- Per-session fiber spawn (small, contained).
- Refactor `src/bnetd/CMakeLists.txt` into library + executable.
- Override `dispatch_frame` with real `handle_*_packet` calls.

## 2026-05-12 (cont. 12) — Legacy build refactor: bnetd_legacy STATIC

Foundation for the real Phase-2 adapter wire-up. The legacy bnetd
build now produces both:

  * `libbnetd_legacy.a` — a STATIC library covering every TU except
    `main.cpp` and `winmain.cpp`, with `PUBLIC` propagation of all
    transitive includes and link deps (common, compat, fmt, win32,
    NETWORK, ZLIB, MYSQL, SQLITE3, PGSQL, ODBC, LUA).
  * `bnetd` — the original executable, now reduced to `main.cpp` +
    `winmain.cpp` + the win32 resource files, linking only
    `bnetd_legacy` privately.

Behaviour is unchanged: `cmake --build build/legacy` still produces
the same `src/bnetd/bnetd` binary at the same path. No source files
were touched.

This is the minimum-viable foothold the v3 strangler-fig adapter
needs: a follow-up step can `target_link_libraries(... bnetd_legacy)`
from the v3 tree and override
`LegacyProtocolHandler::dispatch_frame()` to call the real
`handle_*_packet` symbols.

v3 build/test gates: still **226/226** (no v3 changes this entry).
Legacy build gate: green.


### 2026-05-12 — v3 → bnetd_legacy UDP wire (combined build)

First proof that v3 application code can call into the live legacy
bnetd library. New conditional target `integration_legacy_bnetd_linked`
in `src/v3/CMakeLists.txt`:

  * STATIC, declared only when `TARGET bnetd_legacy` exists (i.e. a
    build configured with both `PVPGN_BUILD_LEGACY=ON` and
    `PVPGN_BUILD_V3=ON`).
  * Adds `${CMAKE_SOURCE_DIR}/src` and `${CMAKE_BINARY_DIR}` to the
    private include path so legacy headers (`common/setup_*.h`,
    `common/packet.h`, `bnetd/handle_udp.h`, generated `config.h`)
    resolve.
  * Compiled with `-w` (legacy headers are not v3-warning-clean).

New files under `src/v3/integration/legacy_bnetd/`:

  * `include/integration/legacy_bnetd/legacy_udp_dispatcher.hpp` —
    `LegacyUdpDispatcher` ctor takes `infra::net::UdpEndpoint&` plus
    a legacy socket fd; `start()` is idempotent.
  * `src/legacy_udp_dispatcher.cpp` — wires `set_on_datagram` to
    build a legacy `t_packet` (`packet_class_udp`,
    `packet_get_raw_data_build`, `packet_set_size`) and forward to
    `pvpgn::bnetd::handle_udp_packet(usock, addr_v4, port, packet)`.
    Mirrors `sd_udpinput()` in legacy `src/bnetd/server.cpp`.

Composition-root contract is documented in the header: constructing
the dispatcher is side-effect free, but `start()` requires the
caller to have run the legacy initialisation path before any
datagram arrives. No unit test is added because the legacy code
reaches into global state that cannot be reasonably stood up from a
Catch2 fixture.

Configure with:

```
cmake -S . -B build/combined -G Ninja \
      -DPVPGN_BUILD_LEGACY=ON -DPVPGN_BUILD_V3=ON
cmake --build build/combined
```

Gates: combined build is green. **226/226 v3 tests pass** under the
combined configuration. Legacy `bnetd` executable still produced.

### 2026-05-12 — Per-session Boost.Fiber spawn API

Optional fiber-style session handler, gated behind
`PVPGN_V3_WITH_FIBER=ON` (the default v3 build is unaffected).

New header `src/v3/infra/net/include/infra/net/fiber_session.hpp`:

  * `SessionChannel` — fiber-side view of one TCP session. Wraps a
    `boost::fibers::buffered_channel<vector<byte>>` for inbound bytes
    plus a `weak_ptr<TcpSession>` for outbound writes. Tracks a
    `dropped` counter for back-pressure visibility.
  * `spawn_session(IoRuntime&, shared_ptr<TcpSession>, handler)` —
    wires the session's `on_bytes` to `try_push` and `on_close` to
    `close_inbox`, then spawns the handler as a fiber on the runtime.
    `start()` is called as part of the helper.

Handler can be written as a synchronous read loop:

```cpp
spawn_session(rt, std::move(session),
              [](SessionChannel& chan) {
                  while (auto chunk = chan.recv()) {
                      chan.send(std::move(*chunk));
                  }
              });
```

**Honest scope note.** Cross-thread Asio↔Fiber wakeups are *not* yet
wired. A fiber blocked on `recv()` only resumes when its own thread
runs the fiber scheduler — which Asio worker threads don't do
between handlers. The header documents this caveat and points at
refactoring-plan-06 as the home for the
`boost::fibers::asio::round_robin` integration. Until then,
`spawn_session` is best used with `IoRuntime::run(1)` so the fiber
and the I/O handler share one thread.

Test `tests/unit/infra/net/fiber_session_test.cpp` exercises
`SessionChannel` in single-thread mode (driver fiber pushes,
consumer fiber pops, round-robin via `boost::this_fiber::yield`).
3 cases:
- Ordered drain of three chunks.
- `recv()` returns nullopt after `close_inbox()`.
- Full-channel pushes increment `dropped` (capacity ⇒ ring of N-1).

Gated under `if(PVPGN_V3_WITH_FIBER)` in
`tests/unit/infra/net/CMakeLists.txt`.

### Build matrix at session end

- `build/v3` — `PVPGN_V3_WITH_FIBER=OFF`, **226/226**.
- `build/v3-fiber` — `PVPGN_V3_WITH_FIBER=ON`, **229/229**.
- `build/combined` — legacy + v3, **226/226** + bnetd_legacy.a +
  integration_legacy_bnetd_linked.a + bnetd executable.
- `build/legacy` — legacy-only, green.

### Deferred

- Asio↔Fiber scheduler integration (`asio::round_robin`) — the next
  step that makes `spawn_session` usable for real loopback traffic.
- Replace bnetd's UDP `fdwatch` path with `UdpEndpoint` +
  `LegacyUdpDispatcher` in `src/bnetd/main.cpp`.
- TCP `handle_*_packet` integration (requires `t_connection*`
  construction, which needs the full legacy server composition root).
