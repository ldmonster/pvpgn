// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file strong_typedef.hpp
/// Minimal strong-typedef helpers.
///
/// Use cases:
///   * Domain IDs: `using AccountId = StrongId<struct AccountIdTag, std::uint32_t>;`
///   * Newtypes:   `using Latency   = StrongTypedef<std::chrono::milliseconds, struct LatencyTag>;`

#include <compare>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>

namespace pvpgn::core {

/// Generic strong-typedef: distinct type per `Tag`, with the same value
/// semantics as `T`. Comparable when `T` is comparable.
template <class T, class Tag>
class StrongTypedef {
public:
    using value_type = T;

    constexpr StrongTypedef() = default;
    constexpr explicit StrongTypedef(T v) : v_(std::move(v)) {}

    constexpr const T& get() const noexcept { return v_; }
    constexpr T&       get() noexcept       { return v_; }

    constexpr auto operator<=>(const StrongTypedef&) const = default;

private:
    T v_{};
};

/// Specialised StrongTypedef for integer IDs (most common case).
/// Provides `.value()` + hashable helper.
template <class Tag, class Underlying = std::uint32_t>
class StrongId {
    static_assert(std::is_integral_v<Underlying>, "StrongId requires integral type");

public:
    using value_type = Underlying;

    constexpr StrongId() = default;
    constexpr explicit StrongId(Underlying v) noexcept : v_(v) {}

    constexpr Underlying value() const noexcept { return v_; }
    constexpr bool       is_valid() const noexcept { return v_ != Underlying{}; }

    constexpr auto operator<=>(const StrongId&) const = default;

private:
    Underlying v_{};
};

}  // namespace pvpgn::core

/// Hash specialisations so StrongId can be used as map key.
namespace std {
template <class Tag, class U>
struct hash<pvpgn::core::StrongId<Tag, U>> {
    std::size_t operator()(pvpgn::core::StrongId<Tag, U> id) const noexcept {
        return std::hash<U>{}(id.value());
    }
};
template <class T, class Tag>
struct hash<pvpgn::core::StrongTypedef<T, Tag>> {
    std::size_t operator()(const pvpgn::core::StrongTypedef<T, Tag>& v) const
        noexcept(noexcept(std::hash<T>{}(v.get()))) {
        return std::hash<T>{}(v.get());
    }
};
}  // namespace std
