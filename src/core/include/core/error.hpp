// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file error.hpp
/// Generic status codes and `Error` value type used by `Result<T,E>`.
///
/// We deliberately do not depend on `<system_error>` so that the type
/// works in `constexpr` contexts and on freestanding builds. Domain-specific
/// error enums coexist with this generic one via `Result<T, MyEnum>`.

#include <string>
#include <string_view>
#include <utility>

namespace pvpgn::core {

/// Coarse-grained outcome category. Mirrors a small subset of
/// `std::errc`/gRPC status codes; specific layers add their own enums.
enum class StatusCode {
    Ok                 = 0,
    Cancelled          = 1,
    Unknown            = 2,
    InvalidArgument    = 3,
    NotFound           = 4,
    AlreadyExists      = 5,
    PermissionDenied   = 6,
    Unauthenticated    = 7,
    ResourceExhausted  = 8,
    FailedPrecondition = 9,
    Aborted            = 10,
    OutOfRange         = 11,
    Unimplemented      = 12,
    Internal           = 13,
    Unavailable        = 14,
    DataLoss           = 15,
    DeadlineExceeded   = 16,
    // extended codes
    Conflict           = 17,  ///< resource conflict (e.g. duplicate account)
    RateLimited        = 18,  ///< request rate exceeded
    Timeout            = 19,  ///< operation timed out (preferred over DeadlineExceeded in new code)
    ProtocolError      = 20,  ///< malformed or unexpected protocol message
    NetworkError       = 21,  ///< network-level failure (connect, send, recv)
    ConfigError        = 22,  ///< configuration is invalid or missing
    DependencyFailed   = 23,  ///< a required dependency (DB, service) is unavailable
    SchemaMismatch     = 24,  ///< DB or config schema version mismatch
};

constexpr std::string_view to_string(StatusCode c) noexcept {
    switch (c) {
        case StatusCode::Ok:                 return "Ok";
        case StatusCode::Cancelled:          return "Cancelled";
        case StatusCode::Unknown:            return "Unknown";
        case StatusCode::InvalidArgument:    return "InvalidArgument";
        case StatusCode::NotFound:           return "NotFound";
        case StatusCode::AlreadyExists:      return "AlreadyExists";
        case StatusCode::PermissionDenied:   return "PermissionDenied";
        case StatusCode::Unauthenticated:    return "Unauthenticated";
        case StatusCode::ResourceExhausted:  return "ResourceExhausted";
        case StatusCode::FailedPrecondition: return "FailedPrecondition";
        case StatusCode::Aborted:            return "Aborted";
        case StatusCode::OutOfRange:         return "OutOfRange";
        case StatusCode::Unimplemented:      return "Unimplemented";
        case StatusCode::Internal:           return "Internal";
        case StatusCode::Unavailable:        return "Unavailable";
        case StatusCode::DataLoss:           return "DataLoss";
        case StatusCode::DeadlineExceeded:   return "DeadlineExceeded";
        // extended codes
        case StatusCode::Conflict:           return "Conflict";
        case StatusCode::RateLimited:        return "RateLimited";
        case StatusCode::Timeout:            return "Timeout";
        case StatusCode::ProtocolError:      return "ProtocolError";
        case StatusCode::NetworkError:       return "NetworkError";
        case StatusCode::ConfigError:        return "ConfigError";
        case StatusCode::DependencyFailed:   return "DependencyFailed";
        case StatusCode::SchemaMismatch:     return "SchemaMismatch";
    }
    return "?";
}

/// Generic error: a code + an optional human-readable message.
/// Cheap to move; intentionally not `constexpr` because of `std::string`.
class Error {
public:
    Error() = default;
    explicit Error(StatusCode c, std::string msg = {})
        : code_(c), message_(std::move(msg)) {}

    // accessors are query functions; ignoring them is always a bug.
    [[nodiscard]] StatusCode         code()    const noexcept { return code_; }
    [[nodiscard]] const std::string& message() const noexcept { return message_; }
    [[nodiscard]] bool               is_ok()   const noexcept { return code_ == StatusCode::Ok; }

    friend bool operator==(const Error& a, const Error& b) noexcept {
        return a.code_ == b.code_ && a.message_ == b.message_;
    }

private:
    StatusCode  code_    = StatusCode::Unknown;
    std::string message_;
};

/// Convenience constructor: `make_error(StatusCode::NotFound, "user gone")`.
[[nodiscard]] inline Error make_error(StatusCode c, std::string msg = {}) {
    return Error{c, std::move(msg)};
}

}  // namespace pvpgn::core
