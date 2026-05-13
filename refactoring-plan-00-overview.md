# PvPGN-PRO Refactoring Plan — 00 · Overview & Guiding Principles

> Scope: A long-horizon, comprehensive refactoring of the `pvpgn-server` codebase
> (~143k LOC of C/C++03-style C++11) into a modern, layered, testable, and
> extensible Battle.net/WOL emulation platform.  Includes a new web UI/API and
> the introduction of Boost.Fiber-based cooperative concurrency for the network
> and game-session layers.

---

## 1. Why refactor?

The current source tree has served PvPGN well for ~25 years but exhibits
classic legacy-C symptoms that hurt maintainability, extensibility, security,
and contributor onboarding:

| Symptom | Concrete evidence in repo |
|---|---|
| God-modules with global state | `bnetd/server.cpp`, `bnetd/connection.cpp`, `bnetd/account.cpp` mix lifecycle, I/O, business rules, persistence. |
| Opaque `typedef struct X { ... } t_x;` with `*_INTERNAL_ACCESS` macro tricks | `account.h`, `channel.h`, `connection.h`, `game.h`. |
| C-style "vtable" via function pointers, only two impls (file/SQL) | `bnetd/storage.h` `t_storage` struct. |
| Macro-injected debug args | `#define account_get_uid(A) account_get_uid_real(A,__FILE__,__LINE__)` and dozens more. |
| Hand-rolled `t_list`, `t_hashtable`, `t_elist`, `t_queue`, `t_bn_int`, `xalloc` | `src/common/list.{cpp,h}`, `hashtable.{cpp,h}`, `xalloc.{cpp,h}`, `xstr.{cpp,h}`, `bn_type.{cpp,h}`. |
| Hand-written `fdwatch_{epoll,kqueue,poll,select}.cpp` reactor | `src/common/fdwatch*.cpp`. |
| Three near-duplicate `main.cpp`/`server.cpp`/`prefs.cpp`/`cmdline.cpp` trees | `src/bnetd`, `src/d2cs`, `src/d2dbs`. |
| Mixed C string handling (`strdup`, `strcpy`, `strncasecmp`, `snprintf` into fixed buffers) | Pervasive — CWE-120/125/134 risk surface. |
| Untestable singletons (`extern t_storage *storage;`, `extern time_t now;`, `prefs_*` globals) | `storage.cpp`, `server.cpp`, `prefs.cpp`. |
| Protocol parsing welded to handler logic | `handle_bnet.cpp`, `handle_irc.cpp`, `handle_wol.cpp`, `handle_d2cs.cpp`. |
| Two tiny tests for >142k LOC | `src/test/CMakeLists.txt`. |
| Manual signal handling, manual `setitimer`, manual SIGPIPE | `bnetd/server.cpp`. |
| No isolation between transport (TCP/UDP socket), session, and protocol FSM | `t_connection` carries socket, queue, account, channel, character, anongame, etc. |
| No HTTP/admin surface | Only a barely-used `handle_apireg` (WOL register), no operator UI. |
| Build coupled to Win32 GUI + 13 storage backends in one `bnetd` binary | `src/bnetd/CMakeLists.txt`. |

## 2. Refactoring goals

1. **SOLID** — every module has a single reason to change; depend on
   abstractions; substitution-safe interfaces (especially storage,
   transport, scripting, protocol).
2. **DDD** — separate **Domain** (Account, Realm, Channel, Game, Clan,
   Ladder, Tournament, Friendship, Ban) from **Application** (use-cases /
   command handlers) from **Infrastructure** (sockets, SQL, files, Lua, HTTP)
   from **Presentation** (bnet protocol, IRC/WOL protocol, D2CS protocol,
   admin REST/WS, CLI).
3. **YAGNI** — drop the half-implemented and unused (e.g. multiple SQL
   backends maintained but not unit-tested, `bnproxy`, broken `bnpcap`,
   unused `bnbot`, `pgetopt`, `gettimeofday` compat for POSIX 2001-era
   systems, custom `xalloc`, `t_bigint` outside crypto, `pugixml` bundled
   when only used as a config file).
4. **DRY** — collapse `bnetd/d2cs/d2dbs` boilerplate (`main`, `prefs`,
   `cmdline`, `handle_signal`, `server`) into a reusable
   `pvpgn::runtime` service-host library; one event loop, one config
   loader, one logger, one CLI parser.
5. **KISS** — replace home-grown containers with the STL/Boost; replace
   the fd-watch abstraction with Boost.Asio + Boost.Fiber; replace
   function-pointer "drivers" with C++ polymorphism + factories.
6. **Expandable** — clear plug-in points for new game protocols
   (StarCraft Remastered, custom W3 mods), new persistence engines
   (Redis, Postgres-async, S3), new scripting runtimes (Lua 5.4,
   QuickJS, sol3-based), and new front-ends (REST, WebSocket, gRPC).
7. **Testable** — every layer compiled as a library, mockable through
   pure-virtual interfaces, with deterministic time/clock injection,
   in-memory repositories, fiber-friendly fake transports. Target
   >70 % line coverage on `domain` and `application`, >50 % on
   `infrastructure`.

## 3. Cross-cutting non-functional targets

| NFR | Target |
|---|---|
| C++ standard | C++20 (concepts, `<format>`, `std::span`, `std::chrono` everywhere, `std::expected`-style result types via `tl::expected` until C++23). |
| Build | CMake ≥ 3.21, presets, `FetchContent` for Boost/Asio/fmt/spdlog/sol3/Catch2, optional vcpkg/Conan. |
| Concurrency | Boost.Asio reactor + Boost.Fiber for per-session logic. No `std::thread` per connection. |
| Memory | `std::unique_ptr`/`std::shared_ptr` only — eliminate `xmalloc/xfree`. RAII for sockets/files/locks. |
| Strings | `std::string`/`std::string_view`; abolish `strcpy`/`sprintf`. |
| Logging | `spdlog` (or `fmtlog`) replacing `eventlog`. Structured JSON sink for ops. |
| Config | Single typed config tree (`toml++` or keep INI but parsed into structs). |
| Errors | `tl::expected<T, Error>` at boundaries, exceptions only for programmer errors. |
| Security | Treat all inbound packets as untrusted: bounded reads, no fixed buffers, fuzz harness (libFuzzer). |
| Portability | Linux, FreeBSD, macOS, Windows (MSVC + clang-cl). No more `WIN32_GUI` baked into core. |
| Observability | `/metrics` (Prometheus), `/healthz`, structured logs, opt-in OpenTelemetry traces. |

## 4. Reading order of this plan

```
00 · Overview & principles                (this file)
01 · Current architecture & code smells
02 · Target architecture & directory layout
03 · Domain model (DDD bounded contexts)
04 · Application layer (use-cases, services)
05 · Infrastructure (persistence, config, logging, time)
06 · Networking & Boost.Fiber integration
07 · Protocol layer (bnet/IRC/WOL/D2/file)
08 · bnetd service refactor
09 · d2cs & d2dbs service refactor
10 · Scripting (Lua) & plug-in system
11 · Testing strategy
12 · Build, tooling, CI
13 · Web UI (admin REST/WS + SPA)
14 · Observability, logging, metrics
15 · Migration roadmap (phased)
16 · Risks, trade-offs, ADRs
```

Each section is self-contained; readers may dive into the section that
matches their role (network engineer → 06+07, DBA → 05, ops/SRE → 13+14,
release manager → 15).

## 5. Non-goals

* Keeping bug-for-bug compatibility with every quirk of the 2003 reference
  server. Wire compatibility with **clients** is preserved, but admin
  tooling, log formats, and config keys may break in the major release.
* Reintroducing dead targets (`bnproxy`, `bnpcap` are slated for archival).
* Multi-master clustering. The plan keeps a single-process write path; HA
  is left as a future ADR.
* Native Windows GUI. The Win32 service wrapper stays; the GUI is replaced
  by the web UI (section 13).

## 6. Versioning

* `2.x` → current stable branch, security fixes only during transition.
* `3.0-alpha` → first release with new layered core + Asio/Fiber, but
  identical client wire protocol and SQL schema.
* `3.x` → web UI, plug-in scripting, optional protocol additions.
* `4.0` → break legacy SQL schema, drop Lua 5.1, drop unused tools.

See [refactoring-plan-15-migration-roadmap.md](refactoring-plan-15-migration-roadmap.md).
