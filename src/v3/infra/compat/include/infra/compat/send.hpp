// SPDX-License-Identifier: GPL-2.0-or-later
//
// Cross-platform `send()` availability shim.
//
// v3 equivalent of src/compat/send.h
//
// The legacy header worked around platforms where `send()` was absent by
// mapping it to `sendto(..., NULL, NULL)`. On all modern POSIX systems
// and on Windows (via <winsock2.h>) `send()` is always available, so the
// workaround is no longer needed.
//
// This header simply ensures the socket API headers are included so that
// `send()` is declared. The v3 tree uses Boost.Asio for all networking;
// this header is provided only as a migration aid. New v3 code must NOT
// call `send()` directly.

#pragma once

#include "infra/compat/socket.hpp"
