# Legacy `src/compat/` survey (post-refactoring snapshot)

This document captures the state of the legacy compat shim layer after
the round of retirements completed during the strangler-fig migration to
v3. It exists to make explicit *which* of the remaining shims still earn
their keep and *which* are de-facto dead weight kept only because they
do no harm.

## What still lives in `src/compat/`

| File | Purpose | Action |
|---|---|---|
| `gethostname.h` | Thin POSIX/Win32 wrapper around `gethostname(2)`. | Keep. |
| `mkdir.h` | Inline `p_mkdir` wrapper over `std::filesystem::create_directory`. | Keep (modernized). |
| `netinet_in.h` | Win32-only `<winsock2.h>` shim; POSIX includes `<netinet/in.h>`. | Keep. |
| `pdir.{h,cpp}` | `<filesystem>`-based directory iterator. | Keep (modernized). |
| `pgetopt.{h,cpp}` | Vendored GNU `getopt_long`. Body gated by `#ifndef HAVE_GETOPT`. | Keep. Compiles to nothing on POSIX (Alpine, glibc, BSD); used only on Win32 + ancient toolchains. |
| `pgetpid.h` | One-liner `getpid()` shim for Win32. | Keep. |
| `psock.{h,cpp}` | Win32 vs POSIX socket abstraction (closesocket, fcntl/ioctl, FD_SET, WSA errno). | Keep. Touching this is its own multi-step project --- see below. |
| `read.h`, `recv.h`, `send.h`, `socket.h` | Win32 SOCKET vs int file-descriptor type punning around `read(2)` / `recv(2)` / `send(2)` / `socket(2)`. | Keep. |
| `rename.h` | Inline `p_rename` wrapper over `std::filesystem::rename`. | Keep (modernized). |
| `runtime_libs.h` | Win32-only `#pragma comment(lib, ...)` directives. | Keep. |
| `stdfileno.h` | `STDIN_FILENO` / `STDOUT_FILENO` / `STDERR_FILENO` macros for Win32. | Keep. |
| `strerror.{h,cpp}` | `pstrerror(errno)` macro on POSIX; full Winsock WSA error decoder on Win32. | Keep. Retiring it would silently regress Win32 error messages (WSA codes are not handled by `std::strerror`). |
| `termios.h` | Win32 stub for the POSIX `termios` interface (used by interactive CLI tools). | Keep. |

## What was retired this round

Five shim pairs deleted. See `plans/step4-checklist.md` for the
per-shim changelog. Summary:

* `access.{h}` --- replaced by `std::filesystem::exists`.
* `statmacros.{h}` --- mode bits dropped from `p_mkdir` API; callers
  no longer pass `S_IRWXU|S_IRGRP|...`.
* `strdup.{h,cpp}` --- callers now `#include <cstring>` and use
  POSIX `strdup` directly.
* `strcasecmp.{h,cpp}` + `strncasecmp.{h,cpp}` --- callers use POSIX
  `<strings.h>` directly (51 files swept).
* `strsep.{h,cpp}` --- one call site rewritten with
  `std::string_view::find`, the other two use a local 6-line helper.
* `mmap.{h,cpp}` --- dead code, no callers.
* `gettimeofday.{h,cpp}` --- two call sites moved to
  `std::chrono::system_clock`.
* `uname.{h,cpp}` --- two call sites use `<sys/utsname.h>` directly
  with `HAVE_UNAME` guards (Win32 branch logs nothing).

## `compat/psock` --- why it stays

`psock.h` is ~9 KB of preprocessor branching that hides four real
differences between POSIX and Win32:

1. **Socket close.** Win32 uses `closesocket`; POSIX uses `close`.
   `compat/psock.h` defines `psock_close`.
2. **Non-blocking I/O.** Win32 uses `ioctlsocket(FIONBIO, ...)`; POSIX
   uses `fcntl(F_SETFL, O_NONBLOCK)`. `psock_ctl` papers over this.
3. **Errno.** Win32 uses `WSAGetLastError`; POSIX uses `errno`.
   `psock_errno` returns the right one, and `psock_errstr` (via
   `strerror.cpp`) decodes both.
4. **Type punning.** Win32 sockets are `SOCKET` (an opaque
   `UINT_PTR`); POSIX sockets are plain `int`. `compat/socket.h`,
   `compat/read.h`, `compat/recv.h`, `compat/send.h` handle the
   `int <-> SOCKET` cast at the I/O boundary.

A clean replacement would be Boost.Asio or `std::experimental::net`,
but both pull in a substantial dependency and touch every connection
loop in `bnetd`, `d2cs`, `d2dbs`. That work belongs in its own plan
(see `plans/refactoring-plan-legacy-bnetd.md` / a future
`plans/refactoring-plan-networking.md`), not as a side track.

## `compat/pgetopt` --- why it stays

`pgetopt.cpp` is 27 KB of vendored GNU `getopt_long`, but the entire
body is gated by:

```c
#ifndef HAVE_GETOPT
... 800 lines ...
#endif /* !HAVE_GETOPT */
```

CMake's `ConfigureChecks.cmake` sets `HAVE_GETOPT` on every POSIX
toolchain (glibc, musl, BSD libc all provide `getopt_long`). On
Alpine, `pgetopt.cpp` therefore compiles to an empty translation
unit. The only platform that actually links the vendored code is
Win32 (MSVC's CRT has no `getopt_long`).

Replacing the call sites with a hand-rolled `std::string_view`-based
parser would buy nothing on POSIX (no code change for the binary) and
would require maintaining a second parser for Win32. Leaving
`compat/pgetopt` alone is the right call until the entire CLI surface
is rewritten (which is also Boost.ProgramOptions territory and out of
scope here).

## What's *not* in `src/compat/` anymore

Headers that used to live here and are now fully retired (deleted, not
moved):

* `access.h`, `statmacros.h`, `strdup.{h,cpp}`, `strcasecmp.{h,cpp}`,
  `strncasecmp.{h,cpp}`, `strsep.{h,cpp}`, `mmap.{h,cpp}`,
  `gettimeofday.{h,cpp}`, `uname.{h,cpp}`.

`src/win32/dirent.h` was also retired in an earlier round (replaced by
`pdir`'s `<filesystem>`-based iterator).
