// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/crypto/big_uint.hpp"

#include <boost/multiprecision/cpp_int.hpp>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace pvpgn::v3::infra::crypto {

namespace mp = boost::multiprecision;

BigUInt BigUInt::from_bytes(std::span<const std::uint8_t> bytes, bool big_endian)
{
    mp::cpp_int v = 0;
    if (big_endian) {
        for (auto b : bytes) {
            v <<= 8;
            v  |= b;
        }
    } else {
        for (std::size_t i = bytes.size(); i-- > 0;) {
            v <<= 8;
            v  |= bytes[i];
        }
    }
    return BigUInt{std::move(v)};
}

std::vector<std::uint8_t> BigUInt::to_bytes(std::size_t byte_count,
                                            bool big_endian) const
{
    std::vector<std::uint8_t> out(byte_count, 0);
    to_bytes(std::span<std::uint8_t>{out.data(), out.size()}, big_endian);
    return out;
}

void BigUInt::to_bytes(std::span<std::uint8_t> out, bool big_endian) const
{
    mp::cpp_int v = value_;
    const std::size_t n = out.size();

    if (big_endian) {
        for (std::size_t i = n; i-- > 0;) {
            out[i] = static_cast<std::uint8_t>(v & 0xffu);
            v >>= 8;
        }
    } else {
        for (std::size_t i = 0; i < n; ++i) {
            out[i] = static_cast<std::uint8_t>(v & 0xffu);
            v >>= 8;
        }
    }

    if (v != 0) {
        throw std::overflow_error{
            "BigUInt::to_bytes: value does not fit in requested width"};
    }
}

BigUInt BigUInt::from_bytes_legacy(std::span<const std::uint8_t> bytes,
                                   int block_size, bool big_endian)
{
    // Step 1: optionally byte-reverse within each block to match the
    // legacy constructor's behaviour for (!bigEndian && blockSize>1).
    std::vector<std::uint8_t> work(bytes.begin(), bytes.end());
    if (!big_endian && block_size > 1) {
        const std::size_t bs = static_cast<std::size_t>(block_size);
        for (std::size_t i = 0; i + bs <= work.size(); i += bs) {
            for (std::size_t j = 0; j < bs / 2; ++j) {
                std::swap(work[i + j], work[i + bs - (j + 1)]);
            }
        }
    }

    // Step 2: interpret `work` exactly the way legacy does for the
    // !big_endian path -- bytes pack into 4-byte segments LSB-first
    // within each segment, with the LAST segment becoming the most
    // significant. This is equivalent to little-endian-within-block
    // and big-endian-across-blocks at the segment level.
    //
    // For the big_endian=true case the legacy code reads input
    // straight as canonical big-endian.
    mp::cpp_int v = 0;
    const std::size_t n = work.size();
    if (big_endian) {
        for (std::size_t i = 0; i < n; ++i) {
            v <<= 8;
            v  |= work[i];
        }
    } else {
        // Translate the legacy segment-packing loop: iterate i from
        // n-1 down to 0, j = i / 4, shifting and adding into the
        // appropriate segment. Build segments[] and assemble.
        const std::size_t seg_bytes = 4;
        const std::size_t seg_count = (n + seg_bytes - 1) / seg_bytes;
        std::vector<std::uint32_t> segments(seg_count, 0);
        for (std::size_t i = n; i-- > 0;) {
            const std::size_t j = i / seg_bytes;
            segments[j] <<= 8;
            segments[j] |= work[i];
        }
        for (std::size_t i = seg_count; i-- > 0;) {
            v <<= 32;
            v  |= segments[i];
        }
    }
    return BigUInt{std::move(v)};
}

void BigUInt::to_bytes_legacy(std::span<std::uint8_t> out, int block_size,
                              bool big_endian) const
{
    // Match legacy `BigInt::getData(out, byteCount, blockSize,
    // bigEndian)` semantics. Phase 1 ALWAYS writes the integer as a
    // sequence of 4-byte segments laid out from the least-significant
    // segment (at out[0]) to the most-significant (at out[size-4]).
    // Within each segment bytes are stored big-endian (MSB at the
    // lower offset). The `bigEndian` flag does NOT affect phase 1
    // -- only the per-block swap below depends on it.
    //
    // Note: when the value's bit-width is significantly smaller than
    // `byteCount * 8`, legacy writes the value at the END of the
    // buffer (because its `segment_count` is per-object). v3 always
    // treats `segment_count = byteCount/4`. For all SRP-3 / Battle.
    // net usage the wire sizes match the operand sizes so this
    // discrepancy never surfaces in practice. The behaviour is
    // documented as out-of-scope for the parity guarantee.
    std::fill(out.begin(), out.end(), std::uint8_t{0});
    mp::cpp_int v = value_;
    constexpr std::size_t SEG = 4;
    const std::size_t n = out.size();
    std::size_t i = 0;
    while (i + SEG <= n) {
        const auto segment = static_cast<std::uint32_t>(v & 0xffffffffu);
        v >>= 32;
        out[i + 0] = static_cast<std::uint8_t>((segment >> 24) & 0xffu);
        out[i + 1] = static_cast<std::uint8_t>((segment >> 16) & 0xffu);
        out[i + 2] = static_cast<std::uint8_t>((segment >> 8)  & 0xffu);
        out[i + 3] = static_cast<std::uint8_t>(segment         & 0xffu);
        i += SEG;
    }
    // Tail (byteCount not a multiple of 4): write remaining low
    // bytes of v at the end of the buffer in big-endian form.
    if (i < n) {
        const std::size_t tail = n - i;
        for (std::size_t b = 0; b < tail; ++b) {
            out[n - 1 - b] = static_cast<std::uint8_t>(v & 0xffu);
            v >>= 8;
        }
    }

    // Phase 2: optionally byte-reverse within each block of size
    // `block_size`. Legacy only does this when `!bigEndian &&
    // blockSize > 1`; we mirror that exactly.
    if (!big_endian && block_size > 1) {
        const std::size_t bs = static_cast<std::size_t>(block_size);
        for (std::size_t off = 0; off + bs <= out.size(); off += bs) {
            for (std::size_t j = 0; j < bs / 2; ++j) {
                std::swap(out[off + j], out[off + bs - (j + 1)]);
            }
        }
    }
}

BigUInt BigUInt::pow_mod(const BigUInt& exp, const BigUInt& modulus) const
{
    return BigUInt{mp::powm(value_, exp.value_, modulus.value_)};
}

BigUInt BigUInt::from_hex(std::string_view hex)
{
    if (hex.empty()) {
        throw std::invalid_argument{"BigUInt::from_hex: empty input"};
    }
    mp::cpp_int v = 0;
    for (char ch : hex) {
        unsigned digit;
        if (ch >= '0' && ch <= '9')
            digit = static_cast<unsigned>(ch - '0');
        else if (ch >= 'a' && ch <= 'f')
            digit = static_cast<unsigned>(ch - 'a' + 10);
        else if (ch >= 'A' && ch <= 'F')
            digit = static_cast<unsigned>(ch - 'A' + 10);
        else
            throw std::invalid_argument{
                "BigUInt::from_hex: non-hex character"};
        v <<= 4;
        v  |= digit;
    }
    return BigUInt{std::move(v)};
}

std::string BigUInt::to_hex() const
{
    std::ostringstream oss;
    oss << std::hex << value_;
    return oss.str();
}

}  // namespace pvpgn::v3::infra::crypto
