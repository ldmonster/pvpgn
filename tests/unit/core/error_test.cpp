// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "core/error.hpp"

using namespace pvpgn::core;

TEST_CASE("Error default construction", "[core][error]") {
    Error e;
    REQUIRE(e.code() == StatusCode::Unknown);
    REQUIRE(e.message().empty());
    REQUIRE_FALSE(e.is_ok());
}

TEST_CASE("Error construction with code and message", "[core][error]") {
    Error e{StatusCode::NotFound, "user not found"};
    REQUIRE(e.code() == StatusCode::NotFound);
    REQUIRE(e.message() == "user not found");
    REQUIRE_FALSE(e.is_ok());
}

TEST_CASE("Error construction with code only", "[core][error]") {
    Error e{StatusCode::InvalidArgument};
    REQUIRE(e.code() == StatusCode::InvalidArgument);
    REQUIRE(e.message().empty());
}

TEST_CASE("Error::is_ok() returns true for Ok code", "[core][error]") {
    Error e{StatusCode::Ok};
    REQUIRE(e.is_ok());
}

TEST_CASE("Error equality comparison", "[core][error]") {
    Error a{StatusCode::NotFound, "msg"};
    Error b{StatusCode::NotFound, "msg"};
    Error c{StatusCode::NotFound, "other"};
    Error d{StatusCode::InvalidArgument, "msg"};
    
    REQUIRE(a == b);
    REQUIRE_FALSE(a == c);
    REQUIRE_FALSE(a == d);
}

TEST_CASE("StatusCode enum values", "[core][error]") {
    REQUIRE(static_cast<int>(StatusCode::Ok) == 0);
    REQUIRE(static_cast<int>(StatusCode::Cancelled) == 1);
    REQUIRE(static_cast<int>(StatusCode::Unknown) == 2);
    REQUIRE(static_cast<int>(StatusCode::InvalidArgument) == 3);
    REQUIRE(static_cast<int>(StatusCode::NotFound) == 4);
    REQUIRE(static_cast<int>(StatusCode::AlreadyExists) == 5);
    REQUIRE(static_cast<int>(StatusCode::PermissionDenied) == 6);
    REQUIRE(static_cast<int>(StatusCode::Unauthenticated) == 7);
    REQUIRE(static_cast<int>(StatusCode::ResourceExhausted) == 8);
    REQUIRE(static_cast<int>(StatusCode::FailedPrecondition) == 9);
    REQUIRE(static_cast<int>(StatusCode::Aborted) == 10);
    REQUIRE(static_cast<int>(StatusCode::OutOfRange) == 11);
    REQUIRE(static_cast<int>(StatusCode::Unimplemented) == 12);
    REQUIRE(static_cast<int>(StatusCode::Internal) == 13);
    REQUIRE(static_cast<int>(StatusCode::Unavailable) == 14);
    REQUIRE(static_cast<int>(StatusCode::DataLoss) == 15);
    REQUIRE(static_cast<int>(StatusCode::DeadlineExceeded) == 16);
}

TEST_CASE("to_string(StatusCode) returns correct names", "[core][error]") {
    REQUIRE(std::string(to_string(StatusCode::Ok)) == "Ok");
    REQUIRE(std::string(to_string(StatusCode::NotFound)) == "NotFound");
    REQUIRE(std::string(to_string(StatusCode::InvalidArgument)) == "InvalidArgument");
    REQUIRE(std::string(to_string(StatusCode::Internal)) == "Internal");
    REQUIRE(std::string(to_string(StatusCode::Unavailable)) == "Unavailable");
}

TEST_CASE("make_error helper function", "[core][error]") {
    auto e = make_error(StatusCode::PermissionDenied, "access denied");
    REQUIRE(e.code() == StatusCode::PermissionDenied);
    REQUIRE(e.message() == "access denied");
}
