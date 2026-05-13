# 01 · Current Architecture & Code Smells

## 1. High-level component map (as-is)

```
                                ┌──────────────────────────┐
   Battle.net clients (TCP) ──► │ bnetd  (src/bnetd)       │ ◄── Lua scripts (lua/)
   WOL / IRC clients (TCP) ───► │   - server loop          │
   File transfer (TCP) ───────► │   - 14 conn_class FSMs   │
   UDP keepalive ─────────────► │   - accounts/clans/teams │
                                │   - channels/games       │
                                │   - ladder/tournament    │
                                │   - storage (file|sql)   │
                                └─────────┬────────────────┘
                                          │ TCP (d2cs↔bnetd protocol)
                                ┌─────────▼──────────────┐
   Diablo 2 clients (TCP) ────► │ d2cs   (src/d2cs)      │
                                │   - realm/character    │
                                │   - GS routing         │
                                └─────────┬──────────────┘
                                          │ TCP (d2cs↔d2dbs protocol)
                                ┌─────────▼──────────────┐
                                │ d2dbs  (src/d2dbs)     │
                                │   - char files on disk │
                                │   - ladder snapshots   │
                                └────────────────────────┘
```

Plus utility binaries: `bniutils` (BNI image tools), `bnpass`, `bnproxy`,
`bntrackd` (UDP master tracker), `client` (`bnchat`, `bnstat`, `bnftp`,
`bnbot`).

## 2. Source tree — observed pathologies

### 2.1 `src/common`
* Mixes **protocol byte layouts** (`bnet_protocol.h`, `irc_protocol.h`,
  `wol_gameres_protocol.h`, `d2cs_*_protocol.h`) with **generic utilities**
  (`list`, `hashtable`, `xalloc`, `tag`, `token`, `xstr`).
* `fdwatch.{cpp,h}` is a tiny event-loop abstraction with four backends
  (`epoll`, `kqueue`, `poll`, `select`) — duplicates Boost.Asio for zero
  benefit.
* `bn_type.{cpp,h}` re-implements integer endianness conversion now in
  `<bit>` / `boost::endian`.
* `pugixml` is vendored into `common/` but used only to parse one config
  asset.
* `setup_before.h` / `setup_after.h` headers wrap each TU to fight macro
  collisions — a code-smell symptom of #include leakage from
  `connection.h`/`account.h` graphs.

### 2.2 `src/bnetd`
* Single `main.cpp` (~700 lines) calls 35+ `*_load`/`*_create`
  init/destroy functions in a hand-ordered list (`accountlist_create`,
  `connlist_create`, `channellist_create`, `gamelist_create`,
  `realmlist_create`, …) — fragile ordering.
* `connection.{cpp,h}` is the worst case: `t_connection` embeds 100+
  fields covering TCP socket, UDP socket, packet queue, account ptr,
  channel ptr, current game, character, anongame state, IRC state, WOL
  state, watch state, quota, last-X-stuff, version-check tag, language,
  D2 game queue, friend-update flags, etc. **One struct, fourteen
  responsibilities.** Mutating it from anywhere is a routine pattern.
* Each `handle_*.cpp` is `if (packet_type == X) { … 200 LOC of business
  logic + replies + DB writes + logging … } else if …`. Hundreds of cases.
* `account_wrap.cpp` is an entire wrapper layer because attributes are
  stored as stringly-typed key/value pairs (`BNET\acct\username`,
  `Record\GAME\0\wins`, …). All schema lives in string constants
  scattered across the source.
* `storage.h` exposes a 20-slot vtable struct of C function pointers
  with stringly-typed paths (`"file:..."`, `"sql:..."`). Adding a backend
  means editing global init and macro guards (`WITH_SQL`).
* `sql_{mysql,odbc,pgsql,sqlite3}.cpp` each duplicate ~1500 lines of
  near-identical query strings; `sql_dbcreator.cpp` parses
  `sql_DB_layout.conf` to create tables — schema migrations are absent.

### 2.3 `src/d2cs`, `src/d2dbs`
* Each has its own `main.cpp`, `cmdline.cpp`, `prefs.cpp`,
  `handle_signal.cpp`, `server.cpp`, `connection.cpp`, `net.cpp` —
  ~80 % copy-paste of `bnetd`.
* `serverqueue.cpp`, `gamequeue.cpp` re-implement a `t_packet *` queue
  already provided by `common/queue.cpp`.

### 2.4 `src/compat`
* Polyfills for `gettimeofday`, `strdup`, `strcasecmp`, `pgetopt`,
  `mmap` — **all standard in any C++11 toolchain**. Pure deletion target.

### 2.5 `lua/`
* Plain Lua 5.1 scripts loaded by `luainterface.cpp` through a hand-rolled
  `luawrapper.cpp`. Object marshalling (`luaobjects.cpp`) is positional
  and brittle: changing C++ struct layout silently breaks scripts.
* No sandboxing; scripts run with full Lua stdlib including `os.execute`.

### 2.6 `src/test`
* Two unit tests (`bnetsrp3_test`, `bigint`). No integration tests, no
  fuzz tests, no protocol replay tests. CI runs only these.

## 3. Cross-cutting code smells

| Smell | Example | Impact |
|---|---|---|
| Globals everywhere | `extern time_t now;`, `extern t_storage *storage;`, `prefs_get_*()` reading process-wide state. | Cannot unit-test in parallel. |
| `t_xxx` typedef-struct opacity via `*_INTERNAL_ACCESS` | `connection.h`, `account.h`, `channel.h`. | Reader has to grep for the macro to see fields; encapsulation is illusory. |
| Macro overloading of functions to inject `__FILE__/__LINE__` | `account_get_uid`, `account_get_name`, `account_get_strattr`. | Hides real function names from debuggers/IDEs. |
| Mixed allocation strategies | `xmalloc` + `new` + `malloc`; ownership unclear. | Leaks (Deleaker is mentioned in README!), use-after-free risk. |
| Manual signal-state flags | `sigexittime`, `do_restart`, `do_save` as `static volatile`. | Race-prone, brittle. |
| C-string fixed buffers | `char dstr[256]; std::strcat(dstr,…)` in `storage_init`. | CWE-120. |
| Stringly-typed attribute keys | `account_get_strattr(acc,"BNET\\acct\\username")`. | Refactor-hostile; no schema. |
| Two-target language mix | C-style headers `extern "C"`-like patterns inside `namespace pvpgn` — no real ABI boundary. | Confusion for new contributors. |
| Logging via printf-style `eventlog(level, fn, fmt, ...)` with fmt v8+ | Uses `fmt::format`, but signature still takes `__FUNCTION__` manually. | Inconsistent log output, missing structured fields. |

## 4. Quantified pain points

```
src/bnetd        : 88 .cpp/.h pairs, ~93k LOC
src/common       : 50 files,        ~22k LOC
src/d2cs         : 21 files,        ~8k LOC
src/d2dbs        : 10 files,        ~3k LOC
src/compat       : 21 files,        ~2k LOC
src/win32        : 6 files,         ~1k LOC
src/bniutils,client,bnpass,bnproxy,bntrackd,json,test : ~13k LOC
                  --------
Total            : ~143k LOC across 250+ files
Test coverage    : 2 unit tests, ~150 LOC.
```

## 5. Implications for the refactor

The current code cannot be incrementally "cleaned in place" because:

1. The header graph is cyclical (`connection.h` ↔ `account.h` ↔
   `channel.h` ↔ `game.h`). Any change ripples through hundreds of files.
2. There is **no seam** between protocol decoding and game logic; we
   cannot stub one without the other. Strangler-pattern wrappers must be
   introduced first (see section 15).
3. Concurrency is hard-coded around a single-thread, single-fdwatch
   reactor. Adding Boost.Fiber requires lifting the I/O abstraction
   first (section 06).

The plan therefore proceeds **top-down (new architecture in `src/v3/`)
and bottom-up (replace `common/` utilities with std/Boost equivalents)**
simultaneously, with the legacy tree continuing to build via shims until
parity is reached. See [refactoring-plan-15-migration-roadmap.md](refactoring-plan-15-migration-roadmap.md).
