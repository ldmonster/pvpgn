// SPDX-License-Identifier: GPL-2.0-or-later
//
// Cross-platform error-number to string conversion.
//
// The legacy code exposed `pstrerror(int errornum)` which:
//   - On POSIX: was a macro alias for `strerror(e)`
//   - On Windows: was a hand-written switch over WSA error codes
//
// This version uses `std::system_error` / `std::generic_category` for
// POSIX errors and `std::system_category` for Windows socket errors,
// returning a `std::string` instead of a raw `const char*`.
//
// Two entry points are provided:
//   `error_string(int errno_val)`   — POSIX errno / generic error
//   `socket_error_string(int code)` — platform socket error code
//                                     (WSAGetLastError() on Windows,
//                                      errno on POSIX)

#pragma once

#include <string>
#include <system_error>

namespace pvpgn::v3::infra::compat {

/// Convert a POSIX `errno` value to a human-readable string.
/// On all platforms this uses `std::generic_category().message()`.
[[nodiscard]] inline std::string error_string(int errno_val) {
    return std::generic_category().message(errno_val);
}

/// Convert a platform socket error code to a human-readable string.
///
/// On POSIX the socket error code IS the errno value, so this is
/// identical to `error_string()`.
///
/// On Windows socket errors are WSA error codes (e.g. WSAECONNRESET)
/// which belong to `std::system_category()`.
[[nodiscard]] inline std::string socket_error_string(int code) {
#if defined(_WIN32) || defined(_WIN64)
    return std::system_category().message(code);
#else
    return std::generic_category().message(code);
#endif
}

}  // namespace pvpgn::v3::infra::compat
