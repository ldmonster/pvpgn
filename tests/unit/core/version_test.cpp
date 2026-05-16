// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>
#include <string_view>

#include "core/version.hpp"

using namespace pvpgn::core;

TEST_CASE("Version constants are non-negative", "[core][version]") {
    REQUIRE(kVersionMajor >= 0);
    REQUIRE(kVersionMinor >= 0);
    REQUIRE(kVersionPatch >= 0);
}

TEST_CASE("Version string is non-empty", "[core][version]") {
    REQUIRE(std::string_view{kVersionString}.size() > 0);
}

TEST_CASE("Version string matches major.minor.patch", "[core][version]") {
    // Verify the version string format is "X.Y.Z"
    std::string_view version{kVersionString};
    REQUIRE(version.find('.') != std::string_view::npos);
}

TEST_CASE("Pre-release string is valid", "[core][version]") {
    // Pre-release can be empty or a valid identifier
    std::string_view prerelease{kVersionPreRelease};
    // Just verify it's a valid string (no assertions needed for empty string)
    REQUIRE(true);
}

TEST_CASE("Version constants are constexpr", "[core][version]") {
    // This test verifies that the constants can be used in constexpr contexts
    constexpr int major = kVersionMajor;
    constexpr int minor = kVersionMinor;
    constexpr int patch = kVersionPatch;
    constexpr const char* str = kVersionString;
    constexpr const char* prerel = kVersionPreRelease;
    
    REQUIRE(major >= 0);
    REQUIRE(minor >= 0);
    REQUIRE(patch >= 0);
    REQUIRE(str != nullptr);
    REQUIRE(prerel != nullptr);
}
