// SPDX-License-Identifier: GPL-2.0-or-later
//
// Cross-platform `recv()` availability shim.
//
// v3 equivalent of src/compat/recv.h
//
// The legacy header worked around platforms where `recv()` was absent by
// mapping it to `recvfrom(..., NULL, NULL)`. On all modern POSIX systems
// and on Windows (via <winsock2.h>) `recv()` is always available, so the
// workaround is no longer needed.
//
// This header simply ensures the socket API headers are included so that
// `recv()` is declared. The v3 tree uses Boost.Asio for all networking;
// this header is provided only as a migration aid. New v3 code must NOT
// call `recv()` directly.

#pragma once

#include "infra/compat/socket.hpp"
