// SPDX-License-Identifier: GPL-2.0-or-later
//
// Cross-platform socket header inclusion shim.
//
// v3 equivalent of src/compat/socket.h
//
// On Windows, <winsock2.h> provides the socket API (equivalent to the
// POSIX <sys/socket.h> + <netinet/in.h> + <arpa/inet.h> + <netdb.h>
// cluster). On POSIX systems those headers are included individually.
//
// The v3 tree uses Boost.Asio for all networking; this header is provided
// only as a migration aid for legacy call sites that are temporarily
// compiled into the v3 build. New v3 code must NOT include this header.

#pragma once

#if defined(_WIN32) || defined(_WIN64)
#  define WIN32_LEAN_AND_MEAN
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <sys/types.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <netdb.h>
#endif
