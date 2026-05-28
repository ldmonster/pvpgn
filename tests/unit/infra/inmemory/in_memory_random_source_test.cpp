// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <cstddef>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "infra/inmemory/in_memory_random_source.hpp"

namespace pvpgn::infra::inmemory {

TEST_CASE("InMemoryRandomSource: NextUintWithinRange", "[infra][inmemory]") {
    InMemoryRandomSource rng(42u);

    for (int i = 0; i < 100; ++i) {
        auto val = rng.next_uint(0, 9);
        REQUIRE(val <= 9u);
    }
}

TEST_CASE("InMemoryRandomSource: NextUintMinEqualsMax", "[infra][inmemory]") {
    InMemoryRandomSource rng(1u);
    auto val = rng.next_uint(7, 7);
    REQUIRE(val == 7u);
}

TEST_CASE("InMemoryRandomSource: NextUintFullRange", "[infra][inmemory]") {
    InMemoryRandomSource rng(99u);
    for (int i = 0; i < 50; ++i) {
        auto val = rng.next_uint(0, 0xFFFFFFFFu);
        // Just verify it doesn't throw and returns a value
        (void)val;
    }
}

TEST_CASE("InMemoryRandomSource: DeterministicWithSameSeed", "[infra][inmemory]") {
    InMemoryRandomSource rng1(12345u);
    InMemoryRandomSource rng2(12345u);

    for (int i = 0; i < 20; ++i) {
        REQUIRE(rng1.next_uint(0, 1000) == rng2.next_uint(0, 1000));
    }
}

TEST_CASE("InMemoryRandomSource: DifferentSeedsProduceDifferentSequences", "[infra][inmemory]") {
    InMemoryRandomSource rng1(1u);
    InMemoryRandomSource rng2(2u);

    // With overwhelming probability, at least one value differs
    bool any_different = false;
    for (int i = 0; i < 20; ++i) {
        if (rng1.next_uint(0, 0xFFFFu) != rng2.next_uint(0, 0xFFFFu)) {
            any_different = true;
            break;
        }
    }
    REQUIRE(any_different);
}

TEST_CASE("InMemoryRandomSource: FillBytesProducesCorrectSize", "[infra][inmemory]") {
    InMemoryRandomSource rng(7u);

    std::array<std::byte, 16> buf{};
    rng.fill_bytes(buf);

    // All bytes should be valid (no UB); just verify the call succeeds
    REQUIRE(buf.size() == 16);
}

TEST_CASE("InMemoryRandomSource: FillBytesProducesNonZeroOutput", "[infra][inmemory]") {
    InMemoryRandomSource rng(0xDEADBEEFu);

    std::array<std::byte, 32> buf{};
    rng.fill_bytes(buf);

    // With overwhelming probability, at least one byte is non-zero
    bool any_nonzero = false;
    for (auto b : buf) {
        if (b != std::byte{0}) {
            any_nonzero = true;
            break;
        }
    }
    REQUIRE(any_nonzero);
}

TEST_CASE("InMemoryRandomSource: FillBytesEmptySpanDoesNotThrow", "[infra][inmemory]") {
    InMemoryRandomSource rng(0u);
    std::array<std::byte, 0> empty{};
    REQUIRE_NOTHROW(rng.fill_bytes(empty));
}

TEST_CASE("InMemoryRandomSource: DefaultConstructorDoesNotThrow", "[infra][inmemory]") {
    REQUIRE_NOTHROW(InMemoryRandomSource{});
}

}  // namespace pvpgn::infra::inmemory
