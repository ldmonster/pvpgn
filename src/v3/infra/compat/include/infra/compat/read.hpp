// SPDX-License-Identifier: GPL-2.0-or-later
//
// Cross-platform `read()` / `_read()` include shim.
//
// v3 equivalent of src/compat/read.h
//
// On POSIX systems `read()` is declared in <unistd.h>.
// On Windows it lives in <io.h> as `_read()`.
//
// The v3 tree avoids raw `read()` calls entirely — I/O goes through
// Boost.Asio async operations. This header exists only as a migration
// aid so that any legacy call site that is temporarily compiled into
// the v3 build can still find the declaration.
//
// New v3 code must NOT include this header; use Boost.Asio instead.

#pragma once

#if defined(_WIN32) || defined(_WIN64)
#  include <io.h>       // _read(), _write(), _close()
#else
#  include <unistd.h>   // read(), write(), close()
#endif
