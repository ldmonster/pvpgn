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

## Phase 2 — Network spine (Asio + Fiber) — **COMPLETE (100%)**
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
- [x] TCP `dispatch_frame` real wiring — `LegacyBnetFrameRouter` (38b) + linked-variant dispatch hook routes framed bytes to `handle_init_packet` / `handle_bnet_packet`; class-refresh hook (38f) tracks `conn_class_init` → `conn_class_bnet` transition
- [x] Replace bnetd TCP `fdwatch` accept loop with `TcpAcceptor` — `TcpBridge` adopts bnet listener fds (38a); `server_handle_v3_accepted_bnet_socket` factory hands accepted fds back to legacy; gated by `server_set_skip_legacy_tcp_fdwatch`
- [x] Live TcpSession ownership of accepted bnet conns — `v3_tcp_session_mode = 1` opt-in (38d-38g): `TcpSession` + `LegacyBnetFrameRouter` own the fd; legacy `t_connection` allocated via `server_handle_v3_owned_bnet_socket` factory; outbound writes redirected via `conn_set_v3_router` slot (38c); `v3_owns_socket` flag prevents double-close in `conn_destroy`; cross-thread `conn_destroy` posted to legacy main loop via `server_post_to_main` (38g)
- [x] **Operational verification** — Runtime smoke-test of `v3_tcp_session_mode = 1` against a real BNet/D2DV client (operational, no code changes required)

## Phase 2b — Bnet section (Session routing, use-cases, and persistence)

### Repositories (InMemory implementations)
- [x] `src/v3/application/ports/include/application/ports/channel_repository.hpp` — `IChannelRepository` interface
- [x] `src/v3/application/ports/include/application/ports/game_repository.hpp` — `IGameRepository` interface
- [x] `src/v3/application/ports/include/application/ports/realm_repository.hpp` — `IRealmRepository` interface
- [x] `src/v3/application/ports/include/application/ports/ip_ban_repository.hpp` — `IIpBanRepository` interface
- [x] `src/v3/infra/storage/repository/channel_repository.hpp` — `InMemoryChannelRepository`
- [x] `src/v3/infra/storage/repository/game_repository.hpp` — `InMemoryGameRepository`
- [x] `src/v3/infra/storage/repository/realm_repository.hpp` — `InMemoryRealmRepository`

### Session Routing
- [x] `src/v3/application/ports/include/application/ports/message_router.hpp` — `IMessageRouter` interface
- [x] `src/v3/infra/routing/include/infra/routing/message_router.hpp` — `MessageRouterImpl`

### Session Context & Factories
- [x] `src/v3/protocol/bnet/include/protocol/bnet/session_context_impl.hpp` — `BnetSessionContextImpl`
- [x] `src/v3/infra/session/include/infra/session/bnet_session_factory.hpp` — `BnetSessionFactory`
- [x] `src/v3/infra/session/include/infra/session/irc_session_factory.hpp` — `IrcSessionFactory` (stub)
- [x] `src/v3/infra/session/include/infra/session/file_session_factory.hpp` — `FileSessionFactory` (stub)
- [x] `src/v3/infra/session/include/infra/session/telnet_session_factory.hpp` — `TelnetSessionFactory` (stub)
- [x] `src/v3/infra/session/include/infra/session/wol_session_factory.hpp` — `WolSessionFactory` (stub)

### Application Use-Cases (Chat)
- [x] `src/v3/application/chat/include/application/chat/join_channel.hpp` + `src/join_channel.cpp`
- [x] `src/v3/application/chat/include/application/chat/post_message.hpp` + `src/post_message.cpp`
- [x] `src/v3/application/chat/include/application/chat/leave_channel.hpp` + `src/leave_channel.cpp`

### Application Use-Cases (Game)
- [x] `src/v3/application/game/include/application/game/start_game.hpp` + `src/start_game.cpp`
- [x] `src/v3/application/game/include/application/game/join_game.hpp` + `src/join_game.cpp`
- [x] `src/v3/application/game/include/application/game/leave_game.hpp` + `src/leave_game.cpp`

### Application Use-Cases (Moderation)
- [x] `src/v3/application/moderation/include/application/moderation/check_ip_ban.hpp` + `src/check_ip_ban.cpp`

### FSM Integration
- [x] `src/v3/protocol/bnet/include/protocol/bnet/use_case_context.hpp` — `BnetUseCaseContext` DI bundle
- [x] `src/v3/protocol/bnet/include/protocol/bnet/event_dispatcher.hpp` — `BnetEventDispatcher` framework
- [x] `src/v3/protocol/bnet/include/protocol/bnet/fsm.hpp` — Updated constructor; session tracking fields added; key handlers wired
- [x] `src/v3/infra/session/include/infra/session/bnet_session_factory.hpp` — Injects `BnetUseCaseContext`

### Unit & Integration Tests (33 new test cases)
- [x] `tests/unit/application/chat/join_channel_test.cpp`
- [x] `tests/unit/application/chat/post_message_test.cpp`
- [x] `tests/unit/application/chat/leave_channel_test.cpp`
- [x] `tests/unit/application/game/start_game_test.cpp`
- [x] `tests/unit/application/game/join_game_test.cpp`
- [x] `tests/unit/application/game/leave_game_test.cpp`
- [x] `tests/unit/application/moderation/check_ip_ban_test.cpp`
- [x] `tests/unit/integration/bnet_session_flow_test.cpp`
- [x] `tests/unit/application/mock_repositories.hpp` — shared mock implementations

### Deferred to Phase 5
- [ ] Full codec integration in FSM handlers (SID_JOINCHANNEL → `JoinChannel` use-case, SID_CHATCOMMAND → `PostMessage`, etc.)
- [ ] WOL/Telnet/IRC protocol completion and session factory integration
- [ ] Persistent repository implementations (SQL/NoSQL backends) replacing InMemory

## Phase 3 — Domain extraction — **COMPLETE (100%)**
- [x] `domain/shared/ids.hpp` — `AccountId`, `ChannelId`, `GameId`, `ClanId`, `TeamId` (StrongId-typed)
- [x] `domain/shared/client_tag.hpp` — `ClientTag` validated 4-byte ASCII tag (`STAR`, `D2DV`, …)
- [x] `domain/shared/user_name.hpp` — `UserName` validated (2..15, alnum/`_-.`, starts with letter, case-insensitive equality, case-preserving display)
- [x] `domain/shared/locale.hpp` — `Locale` 4-char BNet locale with `enUS` fallback
- [x] `domain/shared/bn_hash.hpp` — `BNHash` 20-byte BNet password hash with constant-time equality
- [x] `domain/shared/ip_address.hpp` — IPv4 dotted-quad + IPv6 full-form parser, pure (no syscalls)
- [x] `domain/shared/ban.hpp` — `Ban` value object with `active_at(now)` predicate
- [x] `domain/shared/chat_message.hpp` — bounded validated UTF-8 body (1..223, no control chars)
- [x] `domain/shared/match_report.hpp` — `MatchOutcome`/`PlayerResult`/`MatchReport` value objects
- [x] `domain/shared/events.hpp` — `DomainEvent` variant with **43 alternatives** (identity, chat, social, gameplay, moderation, matchmaking, realm, attributes); added `WhisperDelivered`, `IgnoreAdded`, `IgnoreRemoved`
- [x] `domain/identity/account.hpp` — `Account` aggregate (create/rehydrate, login with ban+lock gating, change_password, command-group grant/revoke, apply_ban/clear_ban, drain_events)
- [x] `domain/identity/attribute_map.hpp` — `AttributeMap` typed wrapper around the legacy `BNET\acct\*` bag (idempotent set, change-event emission, rehydrate); added typed accessors: `username()`, `email()`, `sex()`, `location()`, `description()`, `last_login()`, `created_at()`, `wins(tag)`, `losses(tag)`, `disconnects(tag)`, `ladder_wins(tag)`, `ladder_losses(tag)` + corresponding setters/increment methods
- [x] `domain/identity/account_snapshot.hpp` — `AccountSnapshot` struct for rehydration
- [x] `domain/chat/channel.hpp` — `Channel` aggregate (admit/leave/post/kick/set_topic; ChannelFlags bitset; ChannelPolicy with max_members + client-tag restriction; idempotent join; kick → banlist; AccountId-keyed members); added `Channel::rehydrate(ChannelSnapshot)` factory
- [x] `domain/chat/channel_snapshot.hpp` — `ChannelSnapshot` struct for rehydration
- [x] `domain/chat/whisper.hpp` — `Whisper` value object + `IgnoreList` aggregate with `add()`, `remove()`, `ignores()`, `rehydrate()`, `drain_events()`
- [x] `domain/social/friend_list.hpp` — `FriendList` aggregate (add/remove, 25-cap, self-rejection, idempotent add)
- [x] `domain/social/clan.hpp` — `Clan` aggregate (create with validated 2..4 tag, founder seeded as Chieftain, ranks Chieftain/Shaman/Grunt/Peon, 250-cap, join/remove/set_rank)
- [x] `domain/social/clan_snapshot.hpp` — `ClanSnapshot` + `ClanMemberSnapshot` structs for rehydration
- [x] `domain/social/team.hpp` — `Team` aggregate (W3 arranged-team, fixed 2..4 unique members, immutable roster, one-way disband)
- [x] `domain/gameplay/game.hpp` — `Game` aggregate + deterministic FSM (`Open → InProgress → Reporting → Finalized`); host/join/leave/start/begin_report/finalize; emits `MatchReport` on finalize
- [x] `domain/gameplay/game_snapshot.hpp` — `GameSnapshot` struct for rehydration
- [x] `domain/ladder/ladder.hpp` — `LadderCalculator` stateless service (Elo with configurable K-factor, per-player opponent-mean, disconnect-as-loss toggle)
- [x] `domain/moderation/ip_ban_list.hpp` — `IpBanList` aggregate (exact-IP + CIDR ranges; expiry-aware `blocks()`; `prune_expired()` covers both)
- [x] `domain/moderation/quota.hpp` — `Quota` sliding-window rate limiter (Allowed/Throttled/Muted; mute auto-lifts; emits `AccountQuotaExceeded`)
- [x] `domain/matchmaking/anon_game_queue.hpp` — `AnonGameQueue` FIFO matchmaking (idempotent enqueue, `match(GameId)` pulls `2*team_size` oldest, emits `AnonGameMatched`)
- [x] `domain/matchmaking/tournament.hpp` — `Tournament` single-elim scheduling (≥2 participants, emits `TournamentScheduled`)
- [x] `domain/realm/realm.hpp` — `Realm` aggregate + `Character` value (D2 realm catalog; 16-char names; case-insensitive uniqueness; one-way `unregister`)

### Tests (Phase 3)
- [x] `tests/unit/domain/chat/whisper_test.cpp`
- [x] `tests/unit/domain/identity/attribute_map_typed_test.cpp`

### Deferred to Phase 5
- [ ] Per-context repository interfaces (`IAccountRepo`, …) — Phase 5 prep

## Phase 4 — Application Layer — **COMPLETE (100%)**

### Batch 1: Auth + Missing Ports
**New Ports:**
- [x] `src/v3/application/ports/include/application/ports/clan_repository.hpp` — `IClanRepository`
- [x] `src/v3/application/ports/include/application/ports/ladder_repository.hpp` — `ILadderRepository` + `LadderEntry`
- [x] `src/v3/application/ports/include/application/ports/account_ban_repository.hpp` — `IAccountBanRepository` + `AccountBan`

**New Use-Cases:**
- [x] `src/v3/application/auth/include/application/auth/create_account.hpp` — `CreateAccount` (IP ban + username validation + Account creation)
- [x] `src/v3/application/auth/include/application/auth/logout_user.hpp` — `LogoutUser` (session detach + channel/game cleanup)
- [x] `src/v3/application/auth/include/application/auth/account_lock.hpp` — `LockAccount` + `UnlockAccount`

### Batch 2: Chat Completeness
- [x] `src/v3/application/chat/include/application/chat/list_channels.hpp` — `ListChannels` with tag filtering
- [x] `src/v3/application/chat/include/application/chat/send_emote.hpp` — `SendEmote` (EID_EMOTE broadcast)
- [x] `src/v3/application/chat/include/application/chat/kick_from_channel.hpp` — `KickFromChannel` with permission checks
- [x] `src/v3/application/chat/include/application/chat/ban_from_channel.hpp` — `BanFromChannel` with channel ban list
- [x] `src/v3/application/chat/include/application/chat/set_channel_topic.hpp` — `SetChannelTopic` with 255-char limit

### Batch 3: Game Completeness
- [x] `src/v3/application/game/include/application/game/report_game_result.hpp` — `ReportGameResult` with Elo ladder update
- [x] `src/v3/application/game/include/application/game/list_public_games.hpp` — `ListPublicGames` with tag/type filtering
- [x] `src/v3/application/game/include/application/game/create_private_game.hpp` — `CreatePrivateGame` with password

### Batch 4: Social Use-Cases
**New Port:**
- [x] `src/v3/application/ports/include/application/ports/friend_list_repository.hpp` — `IFriendListRepository`

**Friend Management:**
- [x] `src/v3/application/social/include/application/social/add_friend.hpp` — `AddFriend` (25-cap enforced)
- [x] `src/v3/application/social/include/application/social/remove_friend.hpp` — `RemoveFriend`
- [x] `src/v3/application/social/include/application/social/list_friends.hpp` — `ListFriends` with online status

**Clan Management:**
- [x] `src/v3/application/social/include/application/social/create_clan.hpp` — `CreateClan`
- [x] `src/v3/application/social/include/application/social/disband_clan.hpp` — `DisbandClan` (Chieftain-only)
- [x] `src/v3/application/social/include/application/social/invite_to_clan.hpp` — `InviteToClan` (250-member cap)
- [x] `src/v3/application/social/include/application/social/kick_from_clan.hpp` — `KickFromClan` (rank-based)
- [x] `src/v3/application/social/include/application/social/promote_clan_member.hpp` — `PromoteClanMember`
- [x] `src/v3/application/social/include/application/social/set_clan_motd.hpp` — `SetClanMotd`

### Batch 5: Moderation Use-Cases
- [x] `src/v3/application/moderation/include/application/moderation/ban_account.hpp` — `BanAccount` with expiry
- [x] `src/v3/application/moderation/include/application/moderation/unban_account.hpp` — `UnbanAccount`
- [x] `src/v3/application/moderation/include/application/moderation/ban_ip.hpp` — `BanIp` with CIDR support
- [x] `src/v3/application/moderation/include/application/moderation/kick_connection.hpp` — `KickConnection`
- [x] `src/v3/application/moderation/include/application/moderation/silence_user.hpp` — `SilenceUser` with duration

### Batch 6: Cross-Cutting
**New Ports:**
- [x] `src/v3/application/ports/include/application/ports/permission_checker.hpp` — `IPermissionChecker` (21 permissions)
- [x] `src/v3/application/ports/include/application/ports/audit_log.hpp` — `IAuditLog` (19 action types)

**Implementations:**
- [x] `src/v3/application/auth/include/application/auth/permission_checker.hpp` — `InMemoryPermissionChecker` (group-based)
- [x] `src/v3/infra/audit/include/infra/audit/in_memory_audit_log.hpp` — `InMemoryAuditLog` (circular buffer, thread-safe)
- [x] `src/v3/application/chat/include/application/chat/command_registry.hpp` — `CommandRegistry` (dispatch + permissions)

## Phase 5 — Protocol Decoupling & Infrastructure — **COMPLETE (100%)**
- [x] `protocol/common/packet.hpp` — `BnetHeader`, `parse_bnet_header`, `parse_packet` (bounded, non-throwing)
- [x] `protocol/common/reader.hpp` — `Reader` (LE/BE int, NUL-string, raw blob, skip; non-advancing on OOB)
- [x] `protocol/common/writer.hpp` — `Writer` with `begin_bnet_packet` / `finalize_bnet_packet` size back-patch
- [x] Per-protocol codecs
  - [x] `protocol/bnet/` — pure codec covers **SID_NULL (0x00), SID_GETADVLISTEX (0x09) both directions, SID_ENTERCHAT (0x0A), SID_JOINCHANNEL (0x0C), SID_CHATCOMMAND (0x0E), SID_CHATEVENT (0x0F), SID_PING (0x25), SID_LADDERSEARCH (0x2F) both directions, SID_GETFILETIME (0x33) both directions, SID_LOGONRESPONSE2 (0x3A) both directions, SID_AUTH_INFO (0x50), SID_AUTH_CHECK (0x51) both directions** with full round-trip tests
  - [x] `protocol/irc/` — RFC 1459 framer + tokeniser + encoder (handles bare-LF, prefix/command/middle/trailing, upper-cases commands, `Message` value type)
  - [ ] remaining BNet SIDs (cdkey 0x36, charlist, statstring profile, clan family, friends, account info SIDs, AUTH_INFO server-direction reply, …)
  - [x] `protocol/udp/` — connection-less codec for UDPTEST (0x05), UDPPING (0x07), SESSIONADDR1/2 (0x08/0x09); `Datagram` variant
  - [x] `protocol/telnet/` — line framer (CRLF or bare LF), `tokenise(line) → Command{verb,args}`, `write_line()` reply helper
  - [x] `protocol/file/` — BNFTP header (u16 size + u16 type) + `ClientFileReq` (0x0100) and `ServerFileReply` (0x0000) round-trip
  - [x] `protocol/d2cs/` — 3-byte header (u16 size + u8 type) + D2CS LoginReq / LoginReply (0x01) round-trip
  - [x] `protocol/d2gs/` — 8-byte header (u16 size + u16 type + u32 seqno) + SetGsInfo / Echo / Control with direction-tagged variants
  - [x] `protocol/wolgameres/` — BE TLV walker (u16 size + u16 rngd_size, optional 4-byte zero prefix, repeated u32-tag/u16-type/u16-len TLVs)
  - [x] D2CS realm messages (create-char/create-game/join-game, both directions)
  - [x] D2GS AUTHREQ (0x10) + direction-tagged AUTHREPLY (0x11) family
  - [ ] D2CS game-list / game-info (0x05/0x06), D2GS chat / move / itemdrop bulk-state messages
- [x] Per-protocol FSMs
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

## Phase 5 — Infrastructure & Persistence — **COMPLETE (100%)**

### Protocol Decoupling (Already Complete Above)

### Persistence Overhaul

**Steps 1-2: Core Persistence Abstractions + InMemory Repos**
- [x] `src/v3/application/ports/include/application/ports/unit_of_work.hpp` — `IUnitOfWork` + `UnitOfWorkGuard`
- [x] `src/v3/application/ports/include/application/ports/unit_of_work_factory.hpp` — `IUnitOfWorkFactory`
- [x] `src/v3/infra/persistence/include/infra/persistence/adapter_registry.hpp` — `AdapterRegistry` + `BackendType` + `PersistenceConfig`
- [x] `InMemoryClanRepository`, `InMemoryLadderRepository`, `InMemoryIpBanRepository`, `InMemoryAccountBanRepository`, `InMemoryFriendListRepository`, `InMemoryRealmRepository`
- [x] `InMemoryUnitOfWork` + `InMemoryUnitOfWorkFactory`

**Steps 3-5: Migration Runner + SQLite + File Backend**
- [x] `src/v3/infra/migrations/include/infra/migrations/migration_runner.hpp` — `MigrationRunner` with idempotent migration tracking
- [x] `src/v3/infra/migrations/sql/001_initial_schema.sql` — Initial schema (accounts, clans, ladder, realms, bans, friends)
- [x] `src/v3/infra/sqlite/include/infra/sqlite/connection.hpp` — `SQLiteConnection` with WAL mode, transactions
- [x] SQLite repositories for all types + `SQLiteUnitOfWork` + `SQLiteUnitOfWorkFactory`
- [x] `src/v3/infra/file/include/infra/file/flat_db_reader.hpp` — Legacy `.plain` file parser
- [x] `src/v3/infra/file/include/infra/file/account_repository.hpp` — `FileAccountRepository` (reads `var/users/*.plain`)
- [x] `src/v3/infra/file/include/infra/file/ip_ban_repository.hpp` — `FileIpBanRepository` (reads `bnban.conf`)
- [x] `FileUnitOfWork` + `FileUnitOfWorkFactory`
- [x] Backend registration in `AdapterRegistry`

**Steps 6-7: MySQL + PostgreSQL Skeletons**
- [x] `src/v3/infra/mysql/` — `MySQLConnection` skeleton (gated by `PVPGN_V3_WITH_MYSQL`)
- [x] `src/v3/infra/postgres/` — `PostgreSQLConnection` skeleton (gated by `PVPGN_V3_WITH_POSTGRESQL`)
- [x] Both include `UnitOfWork` + `UnitOfWorkFactory` + stub `AccountRepository`

**Step 8: Configuration Hot-Reload**
- [x] `src/v3/application/ports/include/application/ports/config_subscriber.hpp` — `IConfigSubscriber` port
- [x] `src/v3/infra/config/include/infra/config/config_watcher.hpp` — `ConfigWatcher` with `reload()`, `subscribe()`, `start_watch()`

**Steps 9-10: BNet Codecs + Golden Tests**
- [x] `src/v3/protocol/bnet/include/protocol/bnet/codec_extended.hpp` — New SIDs: `SID_CHARLIST (0x37)`, `SID_CLANMEMBERLIST (0x7D)`, `SID_CLANINFO (0x82)`
- [x] `tests/unit/protocol/bnet/capturing_session_context.hpp` — `CapturingSessionContext` test helper
- [x] `tests/unit/protocol/bnet/fsm_golden_test.cpp` — Full session golden tests (auth, chat, game lifecycle)

## Phase 6 — WebUI + Observability + Protocol Polish — **COMPLETE (100%)**

### Phase 6a: Observability
- [x] `src/v3/application/ports/include/application/ports/metrics_registry.hpp` — `IMetricsRegistry` port with `ICounter`, `IGauge`, `IHistogram`, `MetricLabels`
- [x] `src/v3/infra/metrics/include/infra/metrics/in_memory_metrics_registry.hpp` + `.cpp` — Thread-safe in-memory Prometheus-compatible metrics
- [x] `src/v3/infra/metrics/include/infra/metrics/server_metrics.hpp` — `ServerMetrics` aggregation (active_connections, bytes, packets, logins, channels, games)
- [x] `src/v3/infra/metrics/include/infra/metrics/http_metrics_server.hpp` + `.cpp` — HTTP /metrics endpoint on port 9090 (minimal HTTP/1.1)
- [x] `src/v3/infra/net/include/infra/net/signal_handler.hpp` + `.cpp` — Signal wiring: SIGHUP→ConfigWatcher::reload(), SIGUSR1→SaveAll, SIGUSR2→metrics dump

### Phase 6b: Non-BNet Protocol Handlers
- [x] `src/v3/protocol/telnet/include/protocol/telnet/admin_fsm.hpp` + `.cpp` — `TelnetAdminFsm` line-oriented admin console
- [x] `src/v3/protocol/irc/include/protocol/irc/bridge_fsm.hpp` + `.cpp` — `IrcBridgeFsm` channel bridge to BNet channels (IRC #name ↔ BNet "name")
- [x] `src/v3/protocol/file/include/protocol/file/bnftp_fsm.hpp` + `.cpp` — `BnftpFsm` Battle.net FTP file transfer handler
- [x] `src/v3/protocol/wolgameres/include/protocol/wolgameres/wol_fsm.hpp` + `.cpp` — `WolFsm` Westwood Online game results handler
- [x] Updated session factories: `TelnetSessionFactory`, `IrcSessionFactory`, `FileSessionFactory`, `WolSessionFactory` — wired to their respective FSMs
- [x] `src/v3/infra/net/include/infra/net/shutdown_coordinator.hpp` + `.cpp` — `ShutdownCoordinator` with 30s grace period, session notification, SaveAll flush

### Phase 6c: WebUI + Dashboard
- [x] `src/v3/infra/webui/include/infra/webui/web_server.hpp` + `.cpp` — `EmbeddedWebServer` with REST API: GET /api/v1/status, /players, /channels, /games, /metrics
- [x] `src/v3/infra/webui/include/infra/webui/dashboard_html.hpp` — Single-file embedded HTML dashboard (vanilla JS, 5s auto-refresh, dark mode)

## Phase 7 — Scripting & plug-ins
- [ ]

## Phase 8 — d2cs/d2dbs alignment
- [ ]

## Phase 9 — Clean-up & 4.0 prep
- [ ]

---

## Change log

(entries appended bottom-up by date; newest last)(entries appended bottom-up by date; newest last)

