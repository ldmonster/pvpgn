# 02 · Target Architecture & Directory Layout

## 1. Architectural style

A **hexagonal (ports-and-adapters) + DDD layered** monolith composed of
small libraries, with two long-running service binaries (`bnetd`, `d2cs`)
and one helper daemon (`d2dbs`). The legacy reactor is replaced by
Boost.Asio (I/O multiplexing) + Boost.Fiber (cooperative concurrency for
session/business logic).

```
┌─────────────────────────────────────────────────────────────────┐
│ Presentation / Adapters                                         │
│ ┌────────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌─────────────┐ │
│ │ BNet proto │ │ IRC    │ │ WOL    │ │ D2 CS  │ │ HTTP/WS API │ │
│ │ codec+FSM  │ │ codec  │ │ codec  │ │ codec  │ │ + Web SPA   │ │
│ └─────┬──────┘ └───┬────┘ └───┬────┘ └───┬────┘ └──────┬──────┘ │
└───────┼────────────┼──────────┼──────────┼─────────────┼────────┘
        ▼            ▼          ▼          ▼             ▼
┌─────────────────────────────────────────────────────────────────┐
│ Application Layer  (use-cases / command handlers / sagas)       │
│   LoginUser  CreateAccount  JoinChannel  StartGame  ReportResult│
│   BanUser    SendWhisper   CreateClan    UpdateLadder   ...     │
└──────────────────────────┬──────────────────────────────────────┘
                           ▼
┌─────────────────────────────────────────────────────────────────┐
│ Domain Layer  (pure C++, no I/O, no globals)                    │
│   Account · Channel · Game · Clan · Team · Realm · Friend       │
│   Ladder  · Tournament · Ban · Tournament · Character · Session │
│   Value Objects: ClientTag, UserName, BNHash, Locale, IpAddr    │
│   Domain Services: PasswordPolicy, LadderCalculator, IconRules  │
│   Domain Events:  UserLoggedIn, GameEnded, ChannelMessageSent   │
└──────────────────────────┬──────────────────────────────────────┘
                           ▼
┌─────────────────────────────────────────────────────────────────┐
│ Infrastructure / Adapters (outbound)                            │
│ ┌──────────────┐ ┌──────────┐ ┌────────────┐ ┌────────────────┐ │
│ │ Storage      │ │ Logger   │ │ Clock /    │ │ Scripting host │ │
│ │  - file      │ │ (spdlog) │ │ Scheduler  │ │  (sol3 / Lua)  │ │
│ │  - sqlite    │ │          │ │ (asio+fbr) │ │                │ │
│ │  - mysql     │ │ Metrics  │ │ Timer Svc  │ │ Plug-in loader │ │
│ │  - postgres  │ │ (prom)   │ │            │ │                │ │
│ └──────────────┘ └──────────┘ └────────────┘ └────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

### Direction-of-dependency rule

`presentation` → `application` → `domain` ← `infrastructure`.
Domain depends on **nothing** beyond the C++ standard library and a
tiny `core` utility module. Infrastructure implements interfaces
defined in the domain/application layers. This is enforced by the
CMake target graph (a header from `domain/` cannot `#include` from
`infrastructure/` because the target does not link/expose it).

## 2. Proposed top-level layout

```
pvpgn-server/
├── CMakeLists.txt            # root, presets, options
├── CMakePresets.json         # debug/release/asan/tsan/ci
├── cmake/                    # FindXxx, helpers
├── conf/                     # *.conf.in (kept, slimmed)
├── docs/                     # Markdown docs incl. ADRs
├── third_party/              # vendored fallbacks (fmt, sol3)
├── packaging/                # systemd, docker, debian, rpm
├── webui/                    # Web SPA source (separate, see §13)
│   ├── package.json
│   ├── vite.config.ts
│   └── src/
├── src/
│   ├── core/                 # cross-cutting primitives (no domain)
│   │   ├── result.hpp        # tl::expected alias / status type
│   │   ├── strong_typedef.hpp
│   │   ├── bytes.hpp         # std::span<std::byte>
│   │   ├── endian.hpp        # boost::endian thin wrapper
│   │   ├── clock.hpp         # IClock interface + SystemClock
│   │   ├── logging.hpp       # spdlog wrapper, structured fields
│   │   ├── config.hpp        # toml++ wrapper, typed configs
│   │   └── error.hpp
│   │
│   ├── domain/               # PURE — no I/O, no globals
│   │   ├── identity/         # Account aggregate
│   │   ├── chat/             # Channel, Message, Whisper
│   │   ├── gameplay/         # Game, Player, MatchResult
│   │   ├── matchmaking/      # Anongame, Tournament, MapPool
│   │   ├── social/           # Friends, Clan, Team
│   │   ├── ladder/           # LadderEntry, LadderCalculator
│   │   ├── moderation/       # Ban, Kick, Quota, Watch
│   │   ├── realm/            # Realm (D2)
│   │   ├── shared/           # Value Objects (UserName, ClientTag…)
│   │   └── events/           # DomainEvent base + concrete events
│   │
│   ├── application/          # Use cases, orchestration, sagas
│   │   ├── auth/
│   │   │   ├── login_user.{hpp,cpp}
│   │   │   ├── create_account.{hpp,cpp}
│   │   │   └── change_password.{hpp,cpp}
│   │   ├── chat/
│   │   ├── games/
│   │   ├── social/
│   │   ├── admin/            # used by web UI / telnet console
│   │   └── ports/            # repository & service interfaces
│   │       ├── i_account_repository.hpp
│   │       ├── i_channel_repository.hpp
│   │       ├── i_game_repository.hpp
│   │       ├── i_clan_repository.hpp
│   │       ├── i_ladder_repository.hpp
│   │       ├── i_ban_repository.hpp
│   │       ├── i_event_bus.hpp
│   │       ├── i_message_router.hpp
│   │       ├── i_session_registry.hpp
│   │       └── i_script_host.hpp
│   │
│   ├── protocol/             # PURE codecs + FSMs, no sockets
│   │   ├── common/
│   │   │   ├── packet.hpp    # span-based, immutable view
│   │   │   ├── reader.hpp    # safe little-endian reader
│   │   │   └── writer.hpp
│   │   ├── bnet/             # Battle.net wire format
│   │   │   ├── messages.hpp  # struct per packet ID
│   │   │   ├── codec.{hpp,cpp}
│   │   │   └── fsm.{hpp,cpp} # session state machine
│   │   ├── irc/              # IRC + WOL dialects
│   │   ├── wol/
│   │   ├── d2cs/
│   │   ├── d2gs/             # downstream GS protocol
│   │   ├── file/             # bnftp
│   │   └── udp/
│   │
│   ├── infrastructure/
│   │   ├── net/              # Asio + Fiber glue
│   │   │   ├── io_runtime.{hpp,cpp}
│   │   │   ├── tcp_acceptor.{hpp,cpp}
│   │   │   ├── tcp_session.{hpp,cpp}
│   │   │   ├── udp_endpoint.{hpp,cpp}
│   │   │   └── fiber_scheduler.{hpp,cpp}
│   │   ├── persistence/
│   │   │   ├── file/         # legacy flat-file repos
│   │   │   ├── sqlite/
│   │   │   ├── mysql/
│   │   │   ├── postgres/
│   │   │   ├── odbc/         # optional
│   │   │   ├── migrations/   # versioned SQL migrations
│   │   │   └── unit_of_work.hpp
│   │   ├── scripting/
│   │   │   ├── lua/          # sol3-based sandboxed host
│   │   │   └── plugin_loader.{hpp,cpp}
│   │   ├── observability/
│   │   │   ├── prom_exporter.{hpp,cpp}
│   │   │   ├── log_sinks.{hpp,cpp}
│   │   │   └── tracing.{hpp,cpp}
│   │   ├── webapi/           # HTTP/WS adapter (Beast/Crow)
│   │   │   ├── http_server.{hpp,cpp}
│   │   │   ├── ws_hub.{hpp,cpp}
│   │   │   ├── routes/
│   │   │   ├── auth_jwt.{hpp,cpp}
│   │   │   └── openapi.yaml
│   │   └── config_loader/
│   │
│   ├── runtime/              # shared service host
│   │   ├── service_host.{hpp,cpp}  # signals, lifecycle, DI
│   │   ├── cli.{hpp,cpp}
│   │   ├── daemonize.{hpp,cpp}
│   │   ├── win_service.{hpp,cpp}
│   │   └── composition_root.{hpp,cpp}
│   │
│   ├── services/             # entry-point binaries
│   │   ├── bnetd/            # tiny main.cpp wiring runtime
│   │   ├── d2cs/
│   │   └── d2dbs/
│   │
│   ├── tools/                # CLI helpers (former bn* utilities)
│   │   ├── bnipack/          # was bniutils
│   │   ├── bnpass/
│   │   └── bnstat/           # was client/bnstat
│   │
│   └── tests/
│       ├── unit/
│       ├── integration/      # spins runtime against in-memory deps
│       ├── protocol_replay/  # pcap-driven regression tests
│       └── fuzz/             # libFuzzer / AFL++ harnesses
└── lua/                       # default scripts (kept)
```

### Why several libraries instead of one?

* Enforces the dependency direction at link-time.
* Lets `tests/unit/domain` link **only** `core`+`domain` — proving
  the domain has no hidden ties to sockets/SQL.
* Allows `webui` REST handlers to link `application` but not
  `protocol/bnet`, so an admin API call cannot accidentally call into
  the BNet FSM.

## 3. CMake target graph

```
core           ── (header-only mostly + small .cpp)
domain         ── PRIVATE: core
application    ── PRIVATE: domain, core
protocol-*     ── PRIVATE: core   (NOT domain — codecs are pure)
infra-net      ── PRIVATE: core, application      (Boost.Asio, Boost.Fiber)
infra-persist  ── PRIVATE: core, application      (SQLite/MySQL/etc.)
infra-script   ── PRIVATE: core, application      (sol3, Lua)
infra-webapi   ── PRIVATE: core, application      (Boost.Beast)
infra-obs      ── PRIVATE: core                   (prometheus-cpp)
runtime        ── PRIVATE: core, application, infra-*
bnetd          ── PRIVATE: runtime, protocol-bnet, protocol-irc,
                            protocol-wol, protocol-file, protocol-udp
d2cs           ── PRIVATE: runtime, protocol-d2cs, protocol-d2gs
d2dbs          ── PRIVATE: runtime, protocol-d2gs
tools-*        ── PRIVATE: core
tests-*        ── PRIVATE: domain | application | protocol-* | …
```

## 4. Composition root

A single `composition_root.cpp` per service binary wires interfaces to
implementations from a parsed config — the only place where concrete
types meet:

```cpp
ServiceContainer build_bnetd(const Config& cfg, IoRuntime& io)
{
    ServiceContainer c;
    c.clock         = std::make_shared<SystemClock>();
    c.logger        = make_spdlog(cfg.log);
    c.metrics       = make_prometheus(cfg.metrics);
    c.accountRepo   = make_account_repo(cfg.storage, io);    // file|sql
    c.channelRepo   = std::make_shared<InMemoryChannelRepo>();
    c.gameRepo      = std::make_shared<InMemoryGameRepo>();
    c.eventBus      = std::make_shared<InProcessEventBus>(io);
    c.scriptHost    = make_lua_host(cfg.lua, c);
    c.sessionReg    = std::make_shared<SessionRegistry>();
    c.app           = build_application_services(c);         // pure DI
    return c;
}
```

No static globals; every collaborator is injected. The legacy
`prefs_get_*()` calls vanish, replaced by const references to typed
config structs.

## 5. Module boundaries enforcement

* Each library defines an explicit `target_include_directories(... PUBLIC include/)`
  with a single public include directory; private headers go in `src/`.
* `clang-tidy` rule `misc-include-cleaner` + a custom `include-what-you-use`
  CI step prevent header-graph regression.
* `cmake-graphviz` artifact in CI prints the actual graph; PRs that
  introduce a new edge must justify it in the PR template.

## 6. Migration view

Legacy code continues to live in `src/legacy/{bnetd,common,d2cs,d2dbs,compat,win32}`
during the transition (verbatim copy with `pvpgn::legacy` namespace),
and the new `runtime` binary can fall back to legacy handlers behind a
config flag until each subsystem reaches feature parity. See section 15
for the phased plan.
