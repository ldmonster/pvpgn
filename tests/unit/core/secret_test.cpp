// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "core/secret.hpp"

#include <cstdlib>
#include <sstream>
#include <string>

using pvpgn::core::Secret;

// ── operator<< ───────────────────────────────────────────────────────────────

TEST_CASE("Secret<string> operator<< prints ***", "[core][secret]") {
    Secret<std::string> s{"my_password"};
    std::ostringstream oss;
    oss << s;
    REQUIRE(oss.str() == "***");
}

TEST_CASE("Secret<int> operator<< prints ***", "[core][secret]") {
    Secret<int> s{42};
    std::ostringstream oss;
    oss << s;
    REQUIRE(oss.str() == "***");
}

// ── reveal() ─────────────────────────────────────────────────────────────────

TEST_CASE("Secret<string> reveal() returns actual value", "[core][secret]") {
    Secret<std::string> s{"super_secret"};
    REQUIRE(s.reveal() == "super_secret");
}

TEST_CASE("Secret<int> reveal() returns actual value", "[core][secret]") {
    Secret<int> s{1234};
    REQUIRE(s.reveal() == 1234);
}

// ── from_string() — literal ───────────────────────────────────────────────────

TEST_CASE("Secret::from_string with literal value", "[core][secret]") {
    auto s = Secret<std::string>::from_string("literal_value");
    REQUIRE(s.reveal() == "literal_value");
}

TEST_CASE("Secret::from_string with empty literal", "[core][secret]") {
    auto s = Secret<std::string>::from_string("");
    REQUIRE(s.reveal().empty());
}

// ── from_string() — env: prefix ──────────────────────────────────────────────

TEST_CASE("Secret::from_string with env: prefix reads env var", "[core][secret]") {
    // HOME is reliably set on POSIX systems.
    const char* home = std::getenv("HOME");
    if (home == nullptr) {
        SKIP("HOME env var not set — skipping env: test");
    }
    auto s = Secret<std::string>::from_string("env:HOME");
    REQUIRE(s.reveal() == std::string{home});
}

TEST_CASE("Secret::from_string with env: prefix for unset var returns empty", "[core][secret]") {
    // Use a name that is extremely unlikely to be set.
    auto s = Secret<std::string>::from_string("env:PVPGN_TEST_UNSET_VAR_XYZ_12345");
    REQUIRE(s.reveal().empty());
}

// ── from_string() — file: prefix ─────────────────────────────────────────────

TEST_CASE("Secret::from_string with file: prefix for missing file returns empty", "[core][secret]") {
    auto s = Secret<std::string>::from_string("file:/nonexistent/path/to/secret.txt");
    REQUIRE(s.reveal().empty());
}

// ── move semantics ────────────────────────────────────────────────────────────

TEST_CASE("Secret<string> is movable", "[core][secret]") {
    Secret<std::string> s1{"movable"};
    Secret<std::string> s2{std::move(s1)};
    REQUIRE(s2.reveal() == "movable");
}

TEST_CASE("Secret<string> move-assign works", "[core][secret]") {
    Secret<std::string> s1{"original"};
    Secret<std::string> s2{"other"};
    s2 = std::move(s1);
    REQUIRE(s2.reveal() == "original");
}
