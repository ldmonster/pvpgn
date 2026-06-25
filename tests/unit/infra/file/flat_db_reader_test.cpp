// SPDX-License-Identifier: GPL-2.0-or-later

/// @file flat_db_reader_test.cpp
/// Catch2 unit tests for the legacy flat-file (.plain) parser.
///
/// These tests guard the legacy-format interop fix: the original pvpgn server
/// writes each attribute as `"<escaped-key>"="<escaped-value>"` with doubled
/// backslash separators (e.g. `"BNET\\acct\\username"="Joe"`). The reader must
/// strip the surrounding quotes and un-double the backslashes so that
/// get_field({"BNET","acct","username"}) (which builds the single-backslash key
/// `BNET\acct\username`) matches.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "infra/file/flat_db_reader.hpp"

namespace pvpgn::infra::file {

TEST_CASE("flat_db_reader: parses verbatim legacy quoted+escaped lines",
          "[infra][file][legacy]") {
    // VERBATIM bytes a real legacy server writes for account "Joe":
    //   quote, BNET, backslash-backslash, acct, backslash-backslash, username,
    //   quote, =, quote, Joe, quote
    const std::string content =
        "\"BNET\\\\acct\\\\username\"=\"Joe\"\n"
        "\"BNET\\\\acct\\\\userid\"=\"12\"\n";

    const auto kv = parse_account_file(content);

    REQUIRE(get_field(kv, {"BNET", "acct", "username"}) == "Joe");
    REQUIRE(get_numeric_field(kv, {"BNET", "acct", "userid"}) == 12);
}

TEST_CASE("flat_db_reader: still parses v3's own unquoted single-backslash lines",
          "[infra][file][legacy]") {
    // The pre-fix v3 writer emitted unquoted single-backslash lines. Existing
    // on-disk v3 files must keep working.
    const std::string content =
        "BNET\\acct\\username=Joe\n"
        "BNET\\acct\\userid=12\n";

    const auto kv = parse_account_file(content);

    REQUIRE(get_field(kv, {"BNET", "acct", "username"}) == "Joe");
    REQUIRE(get_numeric_field(kv, {"BNET", "acct", "userid"}) == 12);
}

TEST_CASE("flat_db_reader: unescapes an embedded quote in a legacy value",
          "[infra][file][legacy]") {
    // Legacy line for a value containing a double-quote: He said "hi"
    // On disk the value is escaped as:  He said \"hi\"
    const std::string content =
        "\"BNET\\\\acct\\\\sex\"=\"He said \\\"hi\\\"\"\n";

    const auto kv = parse_account_file(content);

    REQUIRE(get_field(kv, {"BNET", "acct", "sex"}) == "He said \"hi\"");
}

TEST_CASE("flat_db_reader: tolerates spacing around '=' and empty values",
          "[infra][file][legacy]") {
    // The original reader accepted `" = "` spacing and an empty value field.
    const std::string content =
        "\"BNET\\\\acct\\\\username\" = \"Joe\"\n"
        "\"BNET\\\\acct\\\\email\"=\"\"\n";

    const auto kv = parse_account_file(content);

    REQUIRE(get_field(kv, {"BNET", "acct", "username"}) == "Joe");
    REQUIRE(get_field(kv, {"BNET", "acct", "email"}).empty());
}

}  // namespace pvpgn::infra::file
