// SPDX-License-Identifier: GPL-2.0-or-later
//
// infra/crypto/big_uint.hpp -- arbitrary-precision unsigned big
// integer with the subset of operations required by Battle.net
// authentication (SRP-3). Implemented as a thin wrapper over
// `boost::multiprecision::cpp_int` so the build does not depend
// on GMP or a hand-rolled big-integer implementation.
//
// Replaces legacy src/common/bigint.{h,cpp}.

#pragma once

#include <boost/multiprecision/cpp_int.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace pvpgn::v3::infra::crypto {

class BigUInt {
public:
    BigUInt() noexcept = default;
    BigUInt(std::uint32_t v) noexcept : value_(v) {}
    BigUInt(std::uint64_t v) noexcept : value_(v) {}

    // Build from raw bytes. `big_endian=true` matches the legacy
    // BigInt constructor's default, `false` matches Battle.net wire
    // format (little-endian).
    static BigUInt from_bytes(std::span<const std::uint8_t> bytes,
                              bool big_endian = false);

    // Build from raw bytes using legacy `BigInt(input, size,
    // blockSize, bigEndian)` semantics. When `blockSize > 1` and
    // `big_endian=false`, the bytes are byte-reversed within each
    // block first. Used by SRP-3 (`blockSize=4, big_endian=false`
    // is the Battle.net wire convention).
    static BigUInt from_bytes_legacy(std::span<const std::uint8_t> bytes,
                                     int block_size, bool big_endian);

    // Parse from a lowercase hex string (no `0x` prefix). Throws
    // `std::invalid_argument` on invalid characters or empty input.
    // Useful for cross-checking against legacy `BigInt::toHexString`.
    [[nodiscard]] static BigUInt from_hex(std::string_view hex);

    // Serialise to a byte buffer of exactly `byte_count` bytes. The
    // value is zero-padded on the high side if it fits; an exception
    // is thrown if the value would not fit.
    [[nodiscard]] std::vector<std::uint8_t>
    to_bytes(std::size_t byte_count, bool big_endian = false) const;

    void to_bytes(std::span<std::uint8_t> out, bool big_endian = false) const;

    // Serialise using legacy `BigInt::getData(byteCount, blockSize,
    // bigEndian)` semantics: first fills `out` big-endian (always),
    // then if `block_size > 1 && !big_endian` byte-reverses inside
    // each block. Truncates silently if the value does not fit (the
    // legacy implementation behaves the same way).
    void to_bytes_legacy(std::span<std::uint8_t> out, int block_size,
                         bool big_endian) const;

    // Modular exponentiation: returns (*this)^exp mod modulus.
    [[nodiscard]] BigUInt pow_mod(const BigUInt& exp, const BigUInt& modulus) const;

    [[nodiscard]] bool is_zero() const noexcept { return value_ == 0; }

    [[nodiscard]] std::string to_hex() const;

    // Direct boost cpp_int access for callers that need additional
    // operations (used by SRP-3 implementation).
    using backend_type = boost::multiprecision::cpp_int;
    backend_type&       backend() noexcept       { return value_; }
    const backend_type& backend() const noexcept { return value_; }

    // Arithmetic / comparison.
    friend BigUInt operator+(const BigUInt& a, const BigUInt& b) { return BigUInt{a.value_ + b.value_}; }
    friend BigUInt operator-(const BigUInt& a, const BigUInt& b) { return BigUInt{a.value_ - b.value_}; }
    friend BigUInt operator*(const BigUInt& a, const BigUInt& b) { return BigUInt{a.value_ * b.value_}; }
    friend BigUInt operator/(const BigUInt& a, const BigUInt& b) { return BigUInt{a.value_ / b.value_}; }
    friend BigUInt operator%(const BigUInt& a, const BigUInt& b) { return BigUInt{a.value_ % b.value_}; }

    friend bool operator==(const BigUInt& a, const BigUInt& b) noexcept { return a.value_ == b.value_; }
    friend bool operator!=(const BigUInt& a, const BigUInt& b) noexcept { return a.value_ != b.value_; }
    friend bool operator<(const BigUInt& a, const BigUInt& b)  noexcept { return a.value_  < b.value_;  }
    friend bool operator>(const BigUInt& a, const BigUInt& b)  noexcept { return a.value_  > b.value_;  }
    friend bool operator<=(const BigUInt& a, const BigUInt& b) noexcept { return a.value_ <= b.value_; }
    friend bool operator>=(const BigUInt& a, const BigUInt& b) noexcept { return a.value_ >= b.value_; }

private:
    explicit BigUInt(backend_type v) noexcept : value_(std::move(v)) {}
    backend_type value_{};
};

}  // namespace pvpgn::v3::infra::crypto
