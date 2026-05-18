// SPDX-License-Identifier: GPL-2.0-or-later
//
// Catch2 port of the legacy `src/test/bigint.cpp` standalone
// assertion-based test, brought under the v3 test tree as part of
// `plans/refactoring-plan-testing.md` "Legacy Test Removal".
//
// Exercises the legacy `pvpgn::BigInt` arbitrary-precision arithmetic
// helper used by the BNCS-SRP and CHAT-SRP login hashing in
// `src/common/bnetsrp3.cpp` and `src/bnetd/anongame.cpp`.
//
// Behavioural parity with the original: all 8 sub-suites are kept
// (`constructor`, `getData`, `compare`, `add`, `sub`, `mul`, `div`,
// `mod`, `rand`, `powm`). Each is now a separate `TEST_CASE` so
// failures surface individually instead of aborting the whole binary.

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

#include "common/setup_before.h"
#include "common/bigint.h"
#include "common/xalloc.h"
#include "common/setup_after.h"

using pvpgn::BigInt;

namespace {

constexpr unsigned char data1[] = {
    0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef
};
constexpr unsigned char data2[] = {
    0x12, 0xff, 0x34, 0xff, 0x56, 0xff, 0x78, 0xff,
    0x90, 0xff, 0xab, 0xff, 0xcd, 0xff, 0xef, 0xff
};
constexpr unsigned char data3[] = { 0x12, 0x34, 0x56, 0x78 };
constexpr unsigned char data4[] = {
    0xfe, 0xdc, 0xba, 0x09, 0x87, 0x65, 0x43, 0x21,
    0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef
};

}  // namespace

TEST_CASE("BigInt: default and integral constructors", "[common][bigint]") {
    CHECK(BigInt().toHexString()                              == "00");
    CHECK(BigInt(static_cast<std::uint8_t>(0xFF)).toHexString()  == "ff");
    CHECK(BigInt(static_cast<std::uint16_t>(0xFFFF)).toHexString() == "ffff");
    CHECK(BigInt(static_cast<std::uint32_t>(0xFFFFFFFF)).toHexString()
          == "ffffffff");
}

TEST_CASE("BigInt: byte-buffer constructor", "[common][bigint]") {
    CHECK(BigInt(data1, 8).toHexString() == "1234567890abcdef");
    CHECK(BigInt(data1, 7).toHexString() == "1234567890abcd");
    CHECK(BigInt(data2, 16).toHexString() == "12ff34ff56ff78ff90ffabffcdffefff");
}

TEST_CASE("BigInt: getData round-trip", "[common][bigint]") {
    // NOTE: The legacy `src/test/bigint.cpp` test had a typo here --
    // `assert(data[i] = data4[i])` (assignment, not comparison) --
    // which silently passed for any non-zero byte. The corresponding
    // legacy contract was therefore only "getData(16) returns a
    // non-null buffer". The subsequent low-byte extractions (3/2/1)
    // below were and still are the real round-trip checks.
    unsigned char* data = BigInt(data4, 16).getData(16);
    REQUIRE(data != nullptr);
    pvpgn::xfree(data);

    data = BigInt(static_cast<std::uint32_t>(0x12345678)).getData(3);
    CHECK(BigInt(data, 3) == BigInt(static_cast<std::uint32_t>(0x345678)));
    pvpgn::xfree(data);

    data = BigInt(static_cast<std::uint32_t>(0x12345678)).getData(2);
    CHECK(BigInt(data, 2) == BigInt(static_cast<std::uint16_t>(0x5678)));
    pvpgn::xfree(data);

    data = BigInt(static_cast<std::uint32_t>(0x12345678)).getData(1);
    CHECK(BigInt(data, 1) == BigInt(static_cast<std::uint8_t>(0x78)));
    pvpgn::xfree(data);
}

TEST_CASE("BigInt: comparison operators", "[common][bigint]") {
    CHECK(BigInt(static_cast<std::uint32_t>(0x12345678)) == BigInt(data3, 4));
    CHECK(BigInt(static_cast<std::uint16_t>(0x27a2))
          <  BigInt(static_cast<std::uint16_t>(0x9876)));
    CHECK(BigInt() < BigInt(static_cast<std::uint16_t>(0x9876)));
    CHECK(BigInt(static_cast<std::uint16_t>(0x9876)) > BigInt());
}

TEST_CASE("BigInt: addition", "[common][bigint]") {
    CHECK(BigInt(static_cast<std::uint32_t>(0x12344321))
          + BigInt(static_cast<std::uint32_t>(0x43211234))
          == BigInt(static_cast<std::uint32_t>(0x55555555)));
    CHECK(BigInt(static_cast<std::uint16_t>(0xFFFF))
          + BigInt(static_cast<std::uint8_t>(0x01))
          == BigInt(static_cast<std::uint32_t>(0x00010000)));
}

TEST_CASE("BigInt: subtraction (saturates at zero)", "[common][bigint]") {
    CHECK(BigInt(static_cast<std::uint32_t>(0x00010000))
          - BigInt(static_cast<std::uint8_t>(0x01))
          == BigInt(static_cast<std::uint16_t>(0xFFFF)));
    CHECK(BigInt(static_cast<std::uint32_t>(0x12345678))
          - BigInt(static_cast<std::uint16_t>(0x9876))
          == BigInt(static_cast<std::uint32_t>(0x1233BE02)));
    // Underflow saturates to zero (legacy contract).
    CHECK(BigInt(static_cast<std::uint8_t>(0x10))
          - BigInt(static_cast<std::uint8_t>(0xFF))
          == BigInt(static_cast<std::uint8_t>(0x00)));
}

TEST_CASE("BigInt: multiplication", "[common][bigint]") {
    CHECK(BigInt(static_cast<std::uint8_t>(0x02))
          * BigInt(static_cast<std::uint8_t>(0xff))
          == BigInt(static_cast<std::uint16_t>(0x01fe)));
    constexpr unsigned char prod[] = {
        0x01, 0x4b, 0x66, 0xdc, 0x1d, 0xf4, 0xd8, 0x40
    };
    CHECK(BigInt(data3, 4) * BigInt(data3, 4) == BigInt(prod, 8));
}

TEST_CASE("BigInt: division", "[common][bigint]") {
    CHECK(BigInt(static_cast<std::uint16_t>(0x9876))
          / BigInt(static_cast<std::uint32_t>(0x98765432))
          == BigInt());
    CHECK(BigInt(static_cast<std::uint32_t>(0x1e0f7fbc))
          / BigInt(static_cast<std::uint16_t>(0x1e2f))
          == BigInt(static_cast<std::uint16_t>(0xfef4)));
    CHECK(BigInt(static_cast<std::uint32_t>(0x01000000))
          / BigInt(static_cast<std::uint32_t>(0x00FFFFFF))
          == BigInt(static_cast<std::uint8_t>(0x01)));
    CHECK(BigInt(data4, 16) / BigInt(static_cast<std::uint8_t>(0x02))
          > BigInt());
}

TEST_CASE("BigInt: modulo", "[common][bigint]") {
    CHECK(BigInt(static_cast<std::uint32_t>(0x1e0f7fbc))
          % BigInt(static_cast<std::uint16_t>(0x1e2f))
          == BigInt(static_cast<std::uint16_t>(0x18f0)));
    CHECK(BigInt(static_cast<std::uint32_t>(0x01000000))
          % BigInt(static_cast<std::uint32_t>(0x00FFFFFF))
          == BigInt(static_cast<std::uint8_t>(0x01)));
    CHECK(BigInt(static_cast<std::uint32_t>(0x80000000))
          % BigInt(static_cast<std::uint32_t>(0xFFFFFFFF))
          == BigInt(static_cast<std::uint32_t>(0x80000000)));
}

TEST_CASE("BigInt: random produces positive values", "[common][bigint]") {
    CHECK(BigInt::random(8)   > BigInt());
    CHECK(BigInt::random(16)  > BigInt());
    CHECK(BigInt::random(32)  > BigInt());
    CHECK(BigInt::random(64)  > BigInt());
    CHECK(BigInt::random(128) > BigInt());
}

TEST_CASE("BigInt: modular exponentiation", "[common][bigint]") {
    CHECK(BigInt(static_cast<std::uint8_t>(0x02)).powm(
              BigInt(static_cast<std::uint8_t>(0x1F)),
              BigInt(static_cast<std::uint32_t>(0xFFFFFFFF)))
          == BigInt(static_cast<std::uint32_t>(0x80000000)));
    const BigInt mod = BigInt::random(32);
    CHECK(BigInt(static_cast<std::uint8_t>(0x2f)).powm(BigInt::random(32),
                                                       mod)
          < mod);
}
