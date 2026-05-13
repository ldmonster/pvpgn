// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "core/result.hpp"

#include <string>

using namespace pvpgn::core;

TEST_CASE("Result holds a value", "[core][result]") {
    Result<int> r{42};
    REQUIRE(r);
    REQUIRE(r.has_value());
    REQUIRE(r.value() == 42);
    REQUIRE(r.value_or(-1) == 42);
}

TEST_CASE("Result holds an error", "[core][result]") {
    Result<int> r{fail(make_error(StatusCode::NotFound, "x"))};
    REQUIRE_FALSE(r);
    REQUIRE(r.error().code() == StatusCode::NotFound);
    REQUIRE(r.error().message() == "x");
    REQUIRE(r.value_or(-1) == -1);
}

TEST_CASE("Result<void> ok / fail", "[core][result]") {
    Status<> ok_status = ok();
    REQUIRE(ok_status);

    Status<> bad{fail(make_error(StatusCode::Internal, "boom"))};
    REQUIRE_FALSE(bad);
    REQUIRE(bad.error().code() == StatusCode::Internal);
}

TEST_CASE("Result::map composes", "[core][result]") {
    Result<int> r{2};
    auto doubled = r.map([](int v) { return v * 2; });
    REQUIRE(doubled);
    REQUIRE(doubled.value() == 4);

    Result<int> e{fail(make_error(StatusCode::Aborted))};
    auto skipped = e.map([](int v) { return v * 2; });
    REQUIRE_FALSE(skipped);
    REQUIRE(skipped.error().code() == StatusCode::Aborted);
}

TEST_CASE("Result::and_then chains", "[core][result]") {
    auto half = [](int v) -> Result<int> {
        if (v % 2) return fail(make_error(StatusCode::InvalidArgument, "odd"));
        return v / 2;
    };
    Result<int> r{8};
    auto out = r.and_then(half).and_then(half);
    REQUIRE(out);
    REQUIRE(out.value() == 2);

    Result<int> r2{7};
    auto out2 = r2.and_then(half);
    REQUIRE_FALSE(out2);
    REQUIRE(out2.error().code() == StatusCode::InvalidArgument);
}

TEST_CASE("Result::map_error transforms", "[core][result]") {
    Result<int, std::string> r{fail(std::string{"oops"})};
    auto mapped = r.map_error([](const std::string& e) { return e.size(); });
    REQUIRE_FALSE(mapped);
    REQUIRE(mapped.error() == 4u);
}
