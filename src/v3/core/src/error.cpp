// SPDX-License-Identifier: GPL-2.0-or-later

/// @file error.cpp
/// Implementation of from_errno() — translates POSIX errno to StatusCode.

#include "core/error.hpp"

#include <cerrno>

namespace pvpgn::core {

[[nodiscard]] StatusCode from_errno(int err_no) noexcept {
    switch (err_no) {
        // NotFound
        case ENOENT:
        case ENODEV:
        case ENXIO:
            return StatusCode::NotFound;

        // PermissionDenied
        case EACCES:
        case EPERM:
            return StatusCode::PermissionDenied;

        // AlreadyExists
        case EEXIST:
            return StatusCode::AlreadyExists;

        // InvalidArgument
        case EINVAL:
        case ERANGE:
        case EDOM:
            return StatusCode::InvalidArgument;

        // ResourceExhausted
        case ENOMEM:
        case ENOSPC:
            return StatusCode::ResourceExhausted;

        // Timeout
        case ETIMEDOUT:
            return StatusCode::Timeout;

        // NetworkError
        case ECONNREFUSED:
        case ECONNRESET:
        case ECONNABORTED:
        case ENETUNREACH:
        case EHOSTUNREACH:
            return StatusCode::NetworkError;

        // ProtocolError
        case EPROTO:
        case EPROTONOSUPPORT:
        case EPROTOTYPE:
            return StatusCode::ProtocolError;

        // Cancelled
        case ECANCELED:
            return StatusCode::Cancelled;

        // Unimplemented
        case ENOTSUP:
#if defined(EOPNOTSUPP) && EOPNOTSUPP != ENOTSUP
        case EOPNOTSUPP:
#endif
            return StatusCode::Unimplemented;

        // Everything else
        default:
            return StatusCode::Internal;
    }
}

}  // namespace pvpgn::core
