# Phase 1 Step 8 — Migrate `src/compat/` to `src/v3/infra/compat/`

Last updated: 2026-05-21 (Round 117)

## Audit Table

| File | Lines | Callers | Tier | Migration Action | v3 Target Path |
|------|-------|---------|------|-----------------|----------------|
| `gethostname.h` | 37 | 1 | A* | **Already done** — covered by `process.hpp` | `infra/compat/process.hpp` |
| `pgetpid.h` | 33 | 3 | A* | **Already done** — covered by `process.hpp` | `infra/compat/process.hpp` |
| `netinet_in.h` | 29 | 1 | A | ✅ R112 — `netinet_in.hpp` created | `infra/compat/include/infra/compat/netinet_in.hpp` |
| `read.h` | 26 | 0 | A | ✅ R112 — `read.hpp` created | `infra/compat/include/infra/compat/read.hpp` |
| `stdfileno.h` | 43 | 4 | A | ✅ R112 — `stdfileno.hpp` created | `infra/compat/include/infra/compat/stdfileno.hpp` |
| `termios.h` | 58 | 0 | A | ✅ R112 — `termios.hpp` created | `infra/compat/include/infra/compat/termios.hpp` |
| `socket.h` | 40 | 2 | A | ✅ R112 — `socket.hpp` created | `infra/compat/include/infra/compat/socket.hpp` |
| `recv.h` | 38 | 1 | A | ✅ R112 — `recv.hpp` created | `infra/compat/include/infra/compat/recv.hpp` |
| `send.h` | 38 | 1 | A | ✅ R112 — `send.hpp` created | `infra/compat/include/infra/compat/send.hpp` |
| `mkdir.h` | 61 | 4 | B | ✅ R112 — `mkdir.hpp` created | `infra/compat/include/infra/compat/mkdir.hpp` |
| `rename.h` | 47 | 4 | B | ✅ R112 — `rename.hpp` created | `infra/compat/include/infra/compat/rename.hpp` |
| `runtime_libs.h` | 48 | 4 | B | ✅ R112 — `runtime_libs.hpp` created | `infra/compat/include/infra/compat/runtime_libs.hpp` |
| `strerror.h` | 36 | 9 | B | ✅ R112 — `strerror.hpp` created | `infra/compat/include/infra/compat/strerror.hpp` |
| `pdir.h` / `pdir.cpp` | 88 / 216 | 8 | C | ✅ R113 — `directory.hpp` created (RAII `DirectoryIterator`, `list_files()`) | `infra/compat/include/infra/compat/directory.hpp` |
| `pgetopt.h` / `pgetopt.cpp` | 112 / 855 | 0 direct | C | ✅ R114 — `getopt.hpp` created (`CommandLineParser` + `parse_args()`) | `infra/compat/include/infra/compat/getopt.hpp` |
| `psock.h` / `psock.cpp` | 308 / 87 | **17** | C | ✅ R115 — `socket.hpp` created (RAII `UniqueSocket`, `IpAddress`, `Port`, `SocketAddress`, `WinsockGuard`; no Boost.Asio) | `infra/net/include/infra/net/socket.hpp` |

> **Tier A\*** = already migrated in a prior round (process.hpp covers both gethostname.h and pgetpid.h).

---

## Caller Count Detail

### `psock.h` — 17 callers (Tier C)
| File | Module |
|------|--------|
| `src/bnetd/main.cpp` | bnetd |
| `src/bnetd/server.cpp` | bnetd |
| `src/bnetd/connection.cpp` | bnetd |
| `src/bnetd/handle_file.cpp` | bnetd |
| `src/bnetd/tracker.cpp` | bnetd |
| `src/bnetd/udptest_send.cpp` | bnetd |
| `src/common/network.cpp` | common |
| `src/common/addr.cpp` | common |
| `src/common/fdwatch_select.h` | common |
| `src/d2cs/connection.cpp` | d2cs |
| `src/d2cs/d2gs.cpp` | d2cs |
| `src/d2cs/handle_d2cs.cpp` | d2cs |
| `src/d2cs/net.cpp` | d2cs |
| `src/d2cs/s2s.cpp` | d2cs |
| `src/d2cs/server.cpp` | d2cs |
| `src/d2dbs/dbserver.cpp` | d2dbs |
| `src/d2dbs/dbspacket.cpp` | d2dbs |

### `pdir.h` — 8 callers (Tier C)
| File | Module |
|------|--------|
| `src/bnetd/account.cpp` | bnetd |
| `src/bnetd/clan.cpp` | bnetd |
| `src/bnetd/i18n.cpp` | bnetd |
| `src/bnetd/luainterface.cpp` | bnetd |
| `src/bnetd/mail.h` | bnetd |
| `src/bnetd/storage_file.cpp` | bnetd |
| `src/bnetd/userlog.cpp` | bnetd |
| `src/d2cs/handle_d2cs.cpp` | d2cs |

### `strerror.h` — 9 callers (Tier B)
| File | Module |
|------|--------|
| `src/bnetd/ladder.cpp` | bnetd |
| `src/bnetd/server.cpp` | bnetd |
| `src/bnetd/tracker.cpp` | bnetd |
| `src/bnetd/udptest_send.cpp` | bnetd |
| `src/common/systemerror.cpp` | common |
| `src/d2cs/net.cpp` | d2cs |
| `src/d2cs/s2s.cpp` | d2cs |
| `src/d2cs/server.cpp` | d2cs |
| `src/d2dbs/dbserver.cpp` | d2dbs |

### `mkdir.h` — 4 callers (Tier B)
`src/bnetd/mail.cpp`, `src/bnetd/userlog.cpp`, `src/d2cs/handle_d2cs.cpp`, `src/d2dbs/dbspacket.cpp`

### `rename.h` — 4 callers (Tier B)
`src/bnetd/game.cpp`, `src/bnetd/storage_file.cpp`, `src/d2dbs/d2ladder.cpp`, `src/d2dbs/dbspacket.cpp`

### `runtime_libs.h` — 4 callers (Tier B)
`src/bnetd/sql_mysql.cpp`, `src/bnetd/sql_odbc.cpp`, `src/bnetd/sql_pgsql.cpp`, `src/bnetd/sql_sqlite3.cpp`

### `stdfileno.h` — 4 callers (Tier A)
`src/bnetd/main.cpp`, `src/bnetd/runprog.cpp`, `src/d2cs/main.cpp`, `src/d2dbs/main.cpp`

### `socket.h` — 2 callers (Tier A)
`src/bnetd/connection.cpp`, `src/common/network.cpp`

### `pgetpid.h` — 3 callers (Tier A*, already done)
`src/bnetd/main.cpp`, `src/d2cs/main.cpp`, `src/d2dbs/main.cpp`

### `gethostname.h` — 1 caller (Tier A*, already done)
`src/bnetd/message.cpp`

### `netinet_in.h` — 1 caller (Tier A)
`src/common/network.cpp`

### `recv.h` — 1 caller (Tier A)
`src/common/network.cpp`

### `send.h` — 1 caller (Tier A)
`src/common/network.cpp`

### `read.h` — 0 callers (Tier A)
*(no current callers — was used for Win32 `_read` compat)*

### `termios.h` — 0 callers (Tier A)
*(no current callers — was used for password input masking)*

### `pgetopt.h` — 0 direct callers (Tier C)
*(included only by `pgetopt.cpp` itself; cmdline.cpp uses system getopt)*

---

## Migration Order

### Round 112 (this round) — Tier A + Tier B headers
All header-only or trivially wrappable modules. No callers updated yet.

**Tier A created:**
- `infra/compat/include/infra/compat/netinet_in.hpp`
- `infra/compat/include/infra/compat/read.hpp`
- `infra/compat/include/infra/compat/stdfileno.hpp`
- `infra/compat/include/infra/compat/termios.hpp`
- `infra/compat/include/infra/compat/socket.hpp`
- `infra/compat/include/infra/compat/recv.hpp`
- `infra/compat/include/infra/compat/send.hpp`

**Tier B created:**
- `infra/compat/include/infra/compat/mkdir.hpp`
- `infra/compat/include/infra/compat/rename.hpp`
- `infra/compat/include/infra/compat/runtime_libs.hpp`
- `infra/compat/include/infra/compat/strerror.hpp`

**Already done (prior rounds):**
- `infra/compat/include/infra/compat/platform.hpp` (R101)
- `infra/compat/include/infra/compat/process.hpp` (R101, covers `gethostname.h` + `pgetpid.h`)

### Round 113 — Tier C: `pdir.h` → `directory.hpp` ✅ COMPLETE
- ✅ Created `infra/compat/include/infra/compat/directory.hpp`
  - `DirectoryEntry` value type (name + full_path, is_directory/is_regular_file helpers)
  - `DirectoryIterator` RAII class (move-only, range-for support, rewind/reset)
  - `open_directory()` → `std::optional<DirectoryIterator>` (no-throw factory)
  - `read_directory()` → `std::optional<DirectoryEntry>` (legacy-compatible advance)
  - `close_directory()` → `void` (resets to end state)
  - `list_files(dir, ext, recursive)` → `std::vector<std::filesystem::path>` (replaces `dir_getfiles()`)
- ✅ Header-only — no `.cpp` needed (all inline via `std::filesystem`)
- ✅ Comment inventory in `src/v3/CMakeLists.txt` updated
- ✅ Tests: `tests/unit/infra/compat/test_directory.cpp` (20 test cases)
- ✅ `tests/unit/infra/compat/CMakeLists.txt` updated with `test_infra_compat_directory` target
- ✅ Caller migration (8 files in `src/bnetd/` + `src/d2cs/`) completed in Round 116

### Round 114 — Tier C: `pgetopt.h` → `getopt.hpp` ✅ COMPLETE
- ✅ Created `infra/compat/include/infra/compat/getopt.hpp`
  - `ArgSpec` value type: `short_name`, `long_name`, `description`, `has_arg` (none/required/optional)
  - `ParseResult` struct: `options` map, `positional` vector, `error` string; `ok()`, `get()`, `has()`
  - `CommandLineParser` class: `add_option()`, `parse()` → `bool`, `get()`, `has()`,
    `positional_args()`, `error()`, `usage(program_name)` → `std::string`
  - `parse_args(argc, argv, vector<ArgSpec>)` → `ParseResult` free function
  - `parse_args(argc, argv, initializer_list<ArgSpec>)` → `ParseResult` overload
  - Supports: `-v`, `-o val`, `-oval`, `--verbose`, `--output=val`, `--output val`, `--`
  - No exceptions — all errors via `ParseResult::error` / `parser.error()`
  - Header-only — no `.cpp` needed
  - `[[nodiscard]]` on all query / factory functions
- ✅ Header-only — no `.cpp` needed
- ✅ Comment inventory in `src/v3/CMakeLists.txt` updated
- ✅ Tests: `tests/unit/infra/compat/test_getopt.cpp` (28 test cases)
  - Empty argv, short flag, short+arg (space and concat), long flag, long+arg (= and space)
  - Mixed short+long, positionals, `--` separator, unknown short/long errors
  - Missing required arg (short and long), long flag with `=` error
  - Optional arg (present/absent), multiple flags, `get()`/`has()` for absent options
  - `usage()` content, `parse_args()` vector + initializer_list overloads
  - `parse()` idempotency, long-only option, `ParseResult` member functions
- ✅ `tests/unit/infra/compat/CMakeLists.txt` updated with `test_infra_compat_getopt` target
- **Callers NOT updated** — 0 direct callers of `pgetopt.h` confirmed; `pgetopt.cpp` is only
  compiled when `!HAVE_GETOPT` (legacy fallback for systems without system getopt)

### Round 115 — Tier C: `psock.h` → `infra/net/socket.hpp` ✅ COMPLETE
- ✅ Created `infra/net/include/infra/net/socket.hpp`
  - **Design decision**: No Boost.Asio — thin C++20 RAII wrapper over POSIX sockets / Winsock2 directly
  - `SocketFd` — platform socket descriptor alias (`int` / `SOCKET`)
  - `kInvalidSocket` — platform invalid sentinel (`-1` / `INVALID_SOCKET`)
  - `IpAddress` — wraps `in_addr`; `from_string()`, `to_string()`, `any()`, `loopback()`,
    `host_order()`, `network_order()`, `operator==`/`!=`
  - `Port` — strong typedef over `uint16_t`; `host_order()`, `network_order()`, `operator==`/`!=`
  - `SocketAddress` — wraps `sockaddr_in`; `from(IpAddress, Port)`, `ip()`, `port()`, `raw()`, `size()`
  - `UniqueSocket` — RAII move-only socket; auto-closes on destruction; `get()`, `valid()`,
    `release()`, `reset()`
  - `WinsockGuard` — RAII `WSAStartup`/`WSACleanup` (no-op on POSIX); `initialized()`
  - `make_tcp_socket()` → `std::optional<UniqueSocket>`
  - `make_udp_socket()` → `std::optional<UniqueSocket>`
  - `bind_socket(UniqueSocket&, SocketAddress)` → `bool`
  - `listen_socket(UniqueSocket&, int backlog)` → `bool`
  - `accept_connection(UniqueSocket&)` → `std::optional<std::pair<UniqueSocket, SocketAddress>>`
  - `connect_socket(UniqueSocket&, SocketAddress)` → `bool`
  - `set_nonblocking(UniqueSocket&, bool)` → `bool`
  - `set_reuse_addr(UniqueSocket&, bool)` → `bool`
  - `socket_send(UniqueSocket&, std::span<const std::byte>, int)` → `std::optional<std::size_t>`
  - `socket_recv(UniqueSocket&, std::span<std::byte>, int)` → `std::optional<std::size_t>`
  - `socket_error()` → `int`
  - `socket_error_string(int)` → `std::string`
  - All factory/query functions marked `[[nodiscard]]`
  - No exceptions — errors via `std::optional<T>` / `bool`
  - Header-only — no `.cpp` needed
  - Namespace: `pvpgn::infra::net` (matches existing `infra/net/` convention)
- ✅ Header compiles cleanly with `g++ -std=c++20 -fsyntax-only` (verified)
- ✅ `src/v3/CMakeLists.txt` comment block updated with `socket.hpp` inventory
- ✅ Tests: `tests/unit/infra/net/test_socket.cpp` (30 test cases)
  - Unit tests (no real sockets): `UniqueSocket` default/move/release/reset,
    `IpAddress` from_string/any/loopback/to_string/byte-order/equality,
    `Port` host/network order/equality, `SocketAddress` from/ip/port/size,
    `WinsockGuard` initialized, `socket_error()` smoke, `socket_error_string()` non-empty
  - Integration tests (`[integration]` tag): `make_tcp_socket`, `make_udp_socket`,
    `set_reuse_addr`, `set_nonblocking`, `bind_socket`, `listen_socket`,
    `accept_connection` (non-blocking, no peer → nullopt)
- ✅ `tests/unit/infra/net/CMakeLists.txt` updated with `test_infra_net_socket` target
- ✅ Caller migration — 10 of 17 files migrated in Round 117; 7 complex files deferred to Round 118+
  - ✅ R117 migrated: `d2gs.cpp`, `handle_d2cs.cpp`, `dbspacket.cpp`, `handle_file.cpp`,
    `tracker.cpp`, `udptest_send.cpp`, `addr.cpp`, `fdwatch_select.h`, `main.cpp`, `network.cpp`
  - ⏳ Deferred: `server.cpp` (bnetd), `connection.cpp` (bnetd), `connection.cpp` (d2cs),
    `net.cpp` (d2cs), `s2s.cpp` (d2cs), `server.cpp` (d2cs), `dbserver.cpp` (d2dbs)
    — reason: full socket lifecycle management, event loops, 50+ psock call sites each

### Round 116 — Tier C: `pdir.h` caller migration ✅ COMPLETE
- ✅ Migrated all 8 callers of `src/compat/pdir.h` to `infra/compat/directory.hpp`
  - `src/bnetd/account.cpp` — include-only swap (no actual Directory usage)
  - `src/bnetd/clan.cpp` — include-only swap (no actual Directory usage)
  - `src/bnetd/userlog.cpp` — include-only swap (no actual Directory usage)
  - `src/bnetd/i18n.cpp` — `dir_getfiles()` → `list_files()` + `filesystem::path` → `string` conversion
  - `src/bnetd/luainterface.cpp` — `dir_getfiles()` → `list_files()` + conversion
  - `src/bnetd/storage_file.cpp` — 3 `Directory`+`read()` loops → `open_directory()`+`read_directory()`;
    teams section: `dentry_str` hoisted to function scope to avoid goto-over-init
  - `src/bnetd/mail.h` — `mutable Directory mdir` → `mutable std::optional<DirectoryIterator> mdir_`
  - `src/bnetd/mail.cpp` — all `mdir` usages migrated; `Directory::OpenError` replaced with
    `DeliverError` (thrown directly from `createOpenDir()`, re-thrown in `deliver()`)
  - `src/d2cs/handle_d2cs.cpp` — 3 sections: existence-check + 2 charlist loops →
    `open_directory()` + `read_directory()` with `charname_str` holding the `string` lifetime
- ✅ `src/bnetd/CMakeLists.txt` — added `if(TARGET infra_compat) target_link_libraries(bnetd_legacy PUBLIC infra_compat) endif()`
- ✅ `src/d2cs/CMakeLists.txt` — same guard added for `d2cs_legacy`
- ✅ Zero remaining `#include "compat/pdir.h"` in `src/bnetd/` and `src/d2cs/`
- ✅ Zero remaining `Directory::` or `dir_getfiles` references in migrated files

---

### Round 117 — Tier C: `psock.h` caller migration (partial) 🔄 IN PROGRESS

- **Goal**: Migrate all 17 callers of `src/compat/psock.h` to standard POSIX socket APIs
  (or the new v3 `infra/net/socket.hpp`). Classify each file; migrate safe files; defer complex ones.
- **Classification**:
  - **Include-only** (no psock symbols in body): `d2gs.cpp`, `handle_d2cs.cpp`, `dbspacket.cpp`
  - **Simple** (macro/type replacements only): `tracker.cpp`, `udptest_send.cpp`, `addr.cpp`, `fdwatch_select.h`
  - **Simple** (single recv call): `handle_file.cpp`
  - **Simple** (init/deinit only): `main.cpp`
  - **Moderate** (recv/send + errno constants): `network.cpp`
  - **Complex** (deferred): `server.cpp` (bnetd), `connection.cpp` (bnetd), `connection.cpp` (d2cs),
    `net.cpp` (d2cs), `s2s.cpp` (d2cs), `server.cpp` (d2cs), `dbserver.cpp` (d2dbs)
- **Symbol mapping applied**:
  - `PSOCK_AF_INET` → `AF_INET`
  - `psock_sendto` → `sendto`
  - `psock_recv` → `recv`
  - `psock_send` → `send`
  - `psock_t_socklen` → `socklen_t`
  - `t_psock_fd_set` → `fd_set`
  - `psock_errno()` → `errno` (POSIX) / `WSAGetLastError()` (Win32)
  - `psock_init()` → `WSAStartup(MAKEWORD(2,2), &wsaData)` (Win32 only)
  - `psock_deinit()` → `WSACleanup()` (Win32 only)
  - `PSOCK_EINTR/EAGAIN/EWOULDBLOCK/ENOMEM/ENOTCONN/ECONNRESET/EPIPE/ENOBUFS` → POSIX equivalents
- **Platform-guarded include pattern** used throughout:
  ```cpp
  #ifdef _WIN32
  #  ifndef WIN32_LEAN_AND_MEAN
  #    define WIN32_LEAN_AND_MEAN
  #  endif
  #  include <winsock2.h>
  #else
  #  include <sys/socket.h>
  #  include <netinet/in.h>
  #  include <arpa/inet.h>
  #endif
  ```
- **Files migrated** (✅ R117):
  - `src/d2cs/d2gs.cpp` — include-only removal; added `<netinet/in.h>` + `<arpa/inet.h>` for `INADDR_ANY`/`ntohl`
  - `src/d2cs/handle_d2cs.cpp` — include-only removal (no psock symbols in body)
  - `src/d2dbs/dbspacket.cpp` — include-only removal (no psock symbols in body)
  - `src/bnetd/handle_file.cpp` — `psock_recv` → `recv`; `<sys/socket.h>` added
  - `src/bnetd/tracker.cpp` — `PSOCK_AF_INET` → `AF_INET`; `psock_sendto` → `sendto`; `psock_t_socklen` → `socklen_t`
  - `src/bnetd/udptest_send.cpp` — same as tracker.cpp; `psock_errno()` → `errno`; `<cerrno>` added
  - `src/common/addr.cpp` — 4× `PSOCK_AF_INET` → `AF_INET`; `psock_init()` call removed (Win32-only no-op);
    `<netdb.h>` added for `gethostbyname`/`getservbyname`; duplicate `<arpa/inet.h>` guard removed
  - `src/common/fdwatch_select.h` — `t_psock_fd_set` → `fd_set`; `<sys/select.h>` added
  - `src/bnetd/main.cpp` — `psock_init()` → `WSAStartup(MAKEWORD(2,2), &wsaData)` under `#ifdef _WIN32`;
    `psock_deinit()` → `WSACleanup()` under `#ifdef _WIN32`; `<winsock2.h>` added to Win32 block
  - `src/common/network.cpp` — `psock_recv` → `recv`; `psock_send` → `send`; `psock_errno()` →
    local `sock_err` (`WSAGetLastError()` / `errno`); all 8 `PSOCK_E*` constants → POSIX equivalents
- **Files deferred** (⏳ Round 118+):
  - `src/bnetd/server.cpp` — 50+ psock calls; full TCP/UDP server lifecycle; event loop
  - `src/bnetd/connection.cpp` — `psock_shutdown`/`psock_close` on `c->socket.tcp_sock` struct member
  - `src/d2cs/connection.cpp` — same pattern as bnetd/connection.cpp
  - `src/d2cs/net.cpp` — socket creation, send/recv, full lifecycle
  - `src/d2cs/s2s.cpp` — `psock_connect`, `psock_getsockname`
  - `src/d2cs/server.cpp` — `psock_accept`, full server accept loop
  - `src/d2dbs/dbserver.cpp` — `select()` event loop with `PSOCK_FD_*` macros
- **Errors encountered and fixed**:
  - `d2gs.cpp`: `INADDR_ANY`/`ntohl` undefined after removing psock.h → added `<netinet/in.h>` + `<arpa/inet.h>`
  - `handle_file.cpp`: `psock_recv` still undefined after include swap → replaced with `recv`
  - `addr.cpp`: `gethostbyname`/`getservbyname`/`hostent`/`servent` undefined → added `<netdb.h>`
  - `addr.cpp`: duplicate `<arpa/inet.h>` (new block + old `#ifdef HAVE_ARPA_INET_H`) → removed old guard
  - `fdwatch_select.h`: `t_psock_fd_set` undefined → replaced with `fd_set`
- **Verification**: `grep -r '#include "compat/psock.h"' src/` confirms exactly 7 remaining files —
  all are the deferred complex files listed above
- **No CMakeLists changes needed** — migrated files use only standard POSIX/system headers
- **`src/compat/psock.h` and `src/compat/psock.cpp` NOT deleted** — 7 deferred callers still need them
- **Phase 1 Step 8 status**: 10/17 psock callers migrated; 7 deferred to Round 118+
- **Next**: Round 118 — Migrate deferred complex psock callers (server.cpp, connection.cpp ×2,
  net.cpp, s2s.cpp, d2cs/server.cpp, dbserver.cpp)

---

## RAII Opportunities Identified

| Legacy Pattern | v3 RAII Replacement | Round |
|---------------|---------------------|-------|
| `psock_socket()` + `psock_close()` | `UniqueSocket` (RAII, move-only) | R115 |
| `Directory` (manual open/close) | `DirectoryRange` (range-based, `std::filesystem`) | R113 |
| `psock_init()` / `psock_deinit()` | `WinsockGuard` RAII (Windows only) | R115 |
| `p_mkdir` / `p_rename` | Already C++17 `std::filesystem` in legacy headers | Done |

---

## Notes

1. **`psock.h` is the critical path** — it touches the most files (17) and is the most deeply integrated. It should be migrated last, after all other compat modules are done, and its v3 home is `infra/net/` (Boost.Asio), not `infra/compat/`.

2. **`pdir.h` already uses `std::filesystem`** in its `.cpp` implementation — the v3 migration is mostly a namespace/API cleanup to use `std::filesystem::directory_iterator` directly.

3. **`mkdir.h` and `rename.h`** already use `std::filesystem` in the legacy headers themselves — the v3 equivalents are trivial re-exports into the `pvpgn::v3::infra::compat` namespace.

4. **`strerror.h`** has a Windows-specific `.cpp` implementation (`pstrerror`). The v3 equivalent uses `std::system_error` / `std::generic_category().message()` which is portable.

5. **`runtime_libs.h`** (dlopen/LoadLibrary) is only used by SQL backends. The v3 migration should use `std::filesystem` + platform-specific dynamic loading, or better yet, link statically in v3.

6. **`pgetopt.h`** has 0 direct callers in the codebase (all `cmdline.cpp` files use system `getopt`). The 855-line `pgetopt.cpp` is only compiled when `!HAVE_GETOPT`. In v3, CLI parsing should use a modern library (CLI11 or Boost.Program_options).
