// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file flat_db_reader.hpp
/// Parser for legacy pvpgn flat-file database format.
/// Reads key=value files from var/users/ directory.

#include <cstdint>
#include <map>
#include <string>
#include <string_view>

namespace pvpgn::infra::file {

/// Parses a single legacy account file (username.plain format).
/// Returns a map of key=value pairs.
std::map<std::string, std::string> parse_account_file(std::string_view file_content);

/// Extracts an account field with hierarchical key path.
/// Example: get_field(map, "BNET", "acct", "username")
/// retrieves value of key "BNET\acct\username"
std::string get_field(
    const std::map<std::string, std::string>& data,
    std::initializer_list<std::string_view> path);

/// Extracts numeric field (stat values like wins, losses).
/// Returns 0 if not found or not a valid number.
std::int64_t get_numeric_field(
    const std::map<std::string, std::string>& data,
    std::initializer_list<std::string_view> path);

}  // namespace pvpgn::infra::file
