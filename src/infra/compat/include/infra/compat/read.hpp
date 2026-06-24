// SPDX-License-Identifier: GPL-2.0-or-later
//
// Cross-platform `read()` / `_read()` include shim.
//
// On POSIX systems `read()` is declared in <unistd.h>.
// On Windows it lives in <io.h> as `_read()`.
//
// The tree avoids raw `read()` calls entirely — I/O goes through
// Boost.Asio async operations. This header exists only so that legacy
// call sites can still find the declaration.
//
// New code must NOT include this header; use Boost.Asio instead.

#pragma once

#if defined(_WIN32) || defined(_WIN64)
#  include <io.h>       // _read(), _write(), _close()
#else
#  include <unistd.h>   // read(), write(), close()
#endif
