// SPDX-License-Identifier: GPL-2.0-or-later
//
// Cross-platform `send()` availability shim.
//
// The legacy header worked around platforms where `send()` was absent by
// mapping it to `sendto(..., NULL, NULL)`. On all modern POSIX systems
// and on Windows (via <winsock2.h>) `send()` is always available, so the
// workaround is no longer needed.
//
// This header simply ensures the socket API headers are included so that
// `send()` is declared. The networking layer uses Boost.Asio; this header
// is provided only for legacy call sites. New code must NOT call `send()`
// directly.

#pragma once

#include "infra/compat/socket.hpp"
