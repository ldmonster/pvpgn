// SPDX-License-Identifier: GPL-2.0-or-later

#include <catch2/catch_test_macros.hpp>

#include "infra/crypto/bnet_hash.hpp"
#include "infra/crypto/bnet_hash_conv.hpp"
#include "infra/crypto/big_uint.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace crypto = pvpgn::v3::infra::crypto;

// ----- true SHA-1 Known-Answer-Test vectors (FIPS 180-1 / RFC 3174) -------
//
// NOTE: the legacy `sha1_hash` has a bug where size==0 returns the
// SHA-1 IV instead of the canonical empty-string digest. We match
// the legacy behaviour bit-for-bit, so we cannot run the canonical
// "empty string" KAT here. The "abc" and 448-bit KATs are both
// non-empty and exercise the real algorithm.

TEST_CASE("sha1 of \"abc\"", "[infra][crypto][sha1]")
{
    REQUIRE(crypto::to_hex(crypto::sha1(std::string_view{"abc"}))
            == "a9993e364706816aba3e25717850c26c9cd0d89d");
}

TEST_CASE("sha1 of 448-bit message", "[infra][crypto][sha1]")
{
    constexpr std::string_view msg{
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"};
    REQUIRE(crypto::to_hex(crypto::sha1(msg))
            == "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
}

// ----- Blizzard broken-SHA1: regression vectors --------------------------
//
// For the empty input, the algorithm performs zero rounds and the
// digest equals the SHA-1 IV. This is the simplest invariant test
// and detects any change to the constants table.

TEST_CASE("blizzard_hash of empty input is the SHA-1 IV",
          "[infra][crypto][blizzard]")
{
    const crypto::BnetDigest d = crypto::blizzard_hash(std::string_view{""});
    REQUIRE(d[0] == 0x67452301u);
    REQUIRE(d[1] == 0xefcdab89u);
    REQUIRE(d[2] == 0x98badcfeu);
    REQUIRE(d[3] == 0x10325476u);
    REQUIRE(d[4] == 0xc3d2e1f0u);
}

TEST_CASE("blizzard_hash is deterministic", "[infra][crypto][blizzard]")
{
    const auto a = crypto::blizzard_hash(std::string_view{"password"});
    const auto b = crypto::blizzard_hash(std::string_view{"password"});
    REQUIRE(a == b);
}

TEST_CASE("blizzard_hash is input-sensitive", "[infra][crypto][blizzard]")
{
    const auto a = crypto::blizzard_hash(std::string_view{"password"});
    const auto b = crypto::blizzard_hash(std::string_view{"Password"});
    REQUIRE_FALSE(a == b);
}

TEST_CASE("blizzard_hash handles multi-block input",
          "[infra][crypto][blizzard]")
{
    // 200 bytes -- exercises the while loop's `inc=64` path multiple
    // times then a final short block.
    std::array<std::uint8_t, 200> buf{};
    for (std::size_t i = 0; i < buf.size(); ++i)
        buf[i] = static_cast<std::uint8_t>(i & 0xffu);

    const auto digest = crypto::blizzard_hash(
        std::span{reinterpret_cast<const std::byte*>(buf.data()), buf.size()});

    // Recompute and verify determinism / no out-of-bounds via valgrind/asan.
    const auto digest2 = crypto::blizzard_hash(
        std::span{reinterpret_cast<const std::byte*>(buf.data()), buf.size()});
    REQUIRE(digest == digest2);
    // All-zero IV means at least one word must have advanced.
    const bool unchanged = (digest[0] == 0x67452301u) && (digest[4] == 0xc3d2e1f0u);
    REQUIRE_FALSE(unchanged);
}

// ----- to_hex / from_hex round-trip --------------------------------------

TEST_CASE("to_hex / from_hex round-trip", "[infra][crypto][hex]")
{
    const crypto::BnetDigest d{0xdeadbeefu, 0x01020304u, 0xa5a5a5a5u,
                               0x00000001u, 0xffffffffu};
    const auto hex   = crypto::to_hex(d);
    REQUIRE(hex.size() == 40);
    const auto round = crypto::from_hex(hex);
    REQUIRE(round.has_value());
    REQUIRE(*round == d);
}

TEST_CASE("from_hex rejects bad input", "[infra][crypto][hex]")
{
    REQUIRE_FALSE(crypto::from_hex("short").has_value());
    REQUIRE_FALSE(crypto::from_hex(std::string(40, 'z')).has_value());
}

TEST_CASE("to_hex_le swaps each word", "[infra][crypto][hex]")
{
    const crypto::BnetDigest d{0x01020304u, 0u, 0u, 0u, 0u};
    const auto hex = crypto::to_hex_le(d);
    REQUIRE(hex.substr(0, 8) == "04030201");
}

// ----- bnet_hash_conv (wire <-> host digest) -----------------------------

TEST_CASE("digest_to_wire serialises words in little-endian order",
          "[infra][crypto][conv]")
{
    const crypto::BnetDigest d{0x01020304u, 0u, 0u, 0u, 0u};
    const auto wire = crypto::digest_to_wire(d);
    REQUIRE(wire[0] == 0x04);
    REQUIRE(wire[1] == 0x03);
    REQUIRE(wire[2] == 0x02);
    REQUIRE(wire[3] == 0x01);
}

TEST_CASE("digest_from_wire / digest_to_wire round-trip",
          "[infra][crypto][conv]")
{
    const crypto::BnetDigest in{0xdeadbeefu, 0x12345678u, 0xa5a5a5a5u,
                                0x00000001u, 0xfffffffeu};
    const auto wire = crypto::digest_to_wire(in);
    const auto out  = crypto::digest_from_wire(wire);
    REQUIRE(out == in);
}

TEST_CASE("digest_from_wire on all-zero wire is all-zero digest",
          "[infra][crypto][conv]")
{
    const crypto::BnetWireHash zero{};
    const auto d = crypto::digest_from_wire(zero);
    for (auto w : d) REQUIRE(w == 0u);
}

// ----- BigUInt -----------------------------------------------------------

TEST_CASE("BigUInt default-constructs to zero", "[infra][crypto][biguint]")
{
    const crypto::BigUInt b;
    REQUIRE(b.is_zero());
}

TEST_CASE("BigUInt arithmetic basics", "[infra][crypto][biguint]")
{
    const crypto::BigUInt a{std::uint32_t{12345}};
    const crypto::BigUInt b{std::uint32_t{67890}};
    REQUIRE((a + b) == crypto::BigUInt{std::uint32_t{80235}});
    REQUIRE((b - a) == crypto::BigUInt{std::uint32_t{55545}});
    REQUIRE((a * crypto::BigUInt{std::uint32_t{2}})
            == crypto::BigUInt{std::uint32_t{24690}});
}

TEST_CASE("BigUInt::pow_mod (2^10 mod 1000 == 24)",
          "[infra][crypto][biguint]")
{
    const crypto::BigUInt base{std::uint32_t{2}};
    const crypto::BigUInt exp{std::uint32_t{10}};
    const crypto::BigUInt mod{std::uint32_t{1000}};
    REQUIRE(base.pow_mod(exp, mod) == crypto::BigUInt{std::uint32_t{24}});
}

TEST_CASE("BigUInt::pow_mod (Fermat's little theorem: 7^(11-1) mod 11 == 1)",
          "[infra][crypto][biguint]")
{
    const crypto::BigUInt base{std::uint32_t{7}};
    const crypto::BigUInt exp{std::uint32_t{10}};
    const crypto::BigUInt mod{std::uint32_t{11}};
    REQUIRE(base.pow_mod(exp, mod) == crypto::BigUInt{std::uint32_t{1}});
}

TEST_CASE("BigUInt::from_bytes / to_bytes round-trip (little-endian)",
          "[infra][crypto][biguint]")
{
    const std::uint8_t in[] = {0x78, 0x56, 0x34, 0x12};  // LE 0x12345678
    const auto v = crypto::BigUInt::from_bytes(in, false);
    REQUIRE(v == crypto::BigUInt{std::uint32_t{0x12345678}});
    const auto out = v.to_bytes(4, false);
    REQUIRE(out.size() == 4);
    REQUIRE(out[0] == 0x78);
    REQUIRE(out[3] == 0x12);
}

TEST_CASE("BigUInt::from_bytes / to_bytes round-trip (big-endian)",
          "[infra][crypto][biguint]")
{
    const std::uint8_t in[] = {0x12, 0x34, 0x56, 0x78};
    const auto v = crypto::BigUInt::from_bytes(in, true);
    REQUIRE(v == crypto::BigUInt{std::uint32_t{0x12345678}});
    const auto out = v.to_bytes(4, true);
    REQUIRE(out[0] == 0x12);
    REQUIRE(out[3] == 0x78);
}

TEST_CASE("BigUInt::to_bytes throws on overflow",
          "[infra][crypto][biguint]")
{
    const crypto::BigUInt v{std::uint32_t{0x100}};
    REQUIRE_THROWS(v.to_bytes(1));
}
