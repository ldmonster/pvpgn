// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file endian.hpp
/// Bounded, span-based little/big-endian integer read/write.
///
/// No dependency on Boost.Endian yet — we use C++20's `std::endian`
/// and `if constexpr` byte-swaps. The Boost integration arrives with
/// the network layer (Phase 2).

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>

#include "core/bytes.hpp"
#include "core/error.hpp"
#include "core/result.hpp"

namespace pvpgn::core {

namespace detail {

template <class T>
constexpr T byte_swap(T v) noexcept {
    static_assert(std::is_integral_v<T>);
    if constexpr (sizeof(T) == 1) {
        return v;
    } else if constexpr (sizeof(T) == 2) {
        return static_cast<T>((v << 8) | ((v >> 8) & 0xff));
    } else if constexpr (sizeof(T) == 4) {
        auto u = static_cast<std::uint32_t>(v);
        u = ((u & 0x000000ffU) << 24) | ((u & 0x0000ff00U) << 8) |
            ((u & 0x00ff0000U) >> 8)  | ((u & 0xff000000U) >> 24);
        return static_cast<T>(u);
    } else if constexpr (sizeof(T) == 8) {
        auto u = static_cast<std::uint64_t>(v);
        u = ((u & 0x00000000000000ffULL) << 56) | ((u & 0x000000000000ff00ULL) << 40) |
            ((u & 0x0000000000ff0000ULL) << 24) | ((u & 0x00000000ff000000ULL) <<  8) |
            ((u & 0x000000ff00000000ULL) >>  8) | ((u & 0x0000ff0000000000ULL) >> 24) |
            ((u & 0x00ff000000000000ULL) >> 40) | ((u & 0xff00000000000000ULL) >> 56);
        return static_cast<T>(u);
    } else {
        static_assert(sizeof(T) <= 8, "unsupported integer size");
        return v;
    }
}

template <class T, std::endian Wire>
T read_endian(ByteView src) noexcept {
    T raw{};
    std::memcpy(&raw, src.data(), sizeof(T));
    if constexpr (Wire == std::endian::native) return raw;
    else                                       return byte_swap(raw);
}

template <class T, std::endian Wire>
void write_endian(ByteSpan dst, T v) noexcept {
    if constexpr (Wire != std::endian::native) v = byte_swap(v);
    std::memcpy(dst.data(), &v, sizeof(T));
}

}  // namespace detail

/// Read `T` from `src` interpreted as little-endian. Returns OutOfRange
/// if `src.size() < sizeof(T)`.
template <class T>
Result<T> read_le(ByteView src) {
    static_assert(std::is_integral_v<T>, "read_le requires integral");
    if (src.size() < sizeof(T))
        return fail(make_error(StatusCode::OutOfRange, "read_le: short buffer"));
    return detail::read_endian<T, std::endian::little>(src);
}

template <class T>
Result<T> read_be(ByteView src) {
    static_assert(std::is_integral_v<T>, "read_be requires integral");
    if (src.size() < sizeof(T))
        return fail(make_error(StatusCode::OutOfRange, "read_be: short buffer"));
    return detail::read_endian<T, std::endian::big>(src);
}

template <class T>
Status<> write_le(ByteSpan dst, T v) {
    static_assert(std::is_integral_v<T>, "write_le requires integral");
    if (dst.size() < sizeof(T))
        return fail(make_error(StatusCode::OutOfRange, "write_le: short buffer"));
    detail::write_endian<T, std::endian::little>(dst, v);
    return ok();
}

template <class T>
Status<> write_be(ByteSpan dst, T v) {
    static_assert(std::is_integral_v<T>, "write_be requires integral");
    if (dst.size() < sizeof(T))
        return fail(make_error(StatusCode::OutOfRange, "write_be: short buffer"));
    detail::write_endian<T, std::endian::big>(dst, v);
    return ok();
}

}  // namespace pvpgn::core
