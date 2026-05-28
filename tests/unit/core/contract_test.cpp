#include <catch2/catch_test_macros.hpp>
#include "core/contract.hpp"

TEST_CASE("PVPGN_VERIFY passes when condition is true", "[contract]") {
    // Must not throw, abort, or terminate
    PVPGN_VERIFY(1 == 1, "one equals one");
    PVPGN_VERIFY(true, "always true");
    REQUIRE(true); // reached
}

TEST_CASE("PVPGN_VERIFY accepts format args", "[contract]") {
    int x = 42;
    PVPGN_VERIFY(x == 42, "x should be 42, got {}", x);
    REQUIRE(true); // reached
}
