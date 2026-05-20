// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file connection_string.hpp
/// Parser for "host[:port]/database" SQL connection strings, shared
/// between the MySQL and Postgres backends so the same input syntax
/// works regardless of which backend is selected.

#include <cstdint>
#include <string>
#include <string_view>

#include "core/error.hpp"
#include "core/result.hpp"

namespace pvpgn::infra::persistence {

/// Parsed components of a SQL connection string.
struct ParsedConnectionString {
    std::string   host;      ///< server hostname or IP, never empty after parse
    std::uint16_t port;      ///< server port, 0 when caller should use backend default
    std::string   database;  ///< database name, never empty after parse
};

/// Parse a `host[:port]/database` connection string.
///
/// Examples:
///   "db.example.com/bnetd"        -> host="db.example.com", port=0, database="bnetd"
///   "127.0.0.1:3306/bnetd"        -> host="127.0.0.1", port=3306, database="bnetd"
///   "/var/run/mysqld.sock/bnetd"  -> host="/var/run/mysqld.sock", port=0, database="bnetd"
///       (UNIX-socket-style host paths keep their internal '/' segments;
///        only the final '/' introduces the database name.)
///
/// Returns InvalidArgument on empty input, missing '/', empty host segment,
/// empty database segment, non-numeric port, trailing ':', or port outside
/// the inclusive range [1, 65535].
core::Result<ParsedConnectionString, core::Error>
parse_connection_string(std::string_view cs);

}  // namespace pvpgn::infra::persistence
