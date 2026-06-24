// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file result.hpp
/// A minimal `Result<T, E>` ("either" / "expected").
///
/// We do not yet require C++23's `std::expected`. This implementation is
/// intentionally small and dependency-free: ~100 LOC, value semantics,
/// monadic `map` / `and_then` / `or_else`, and no exceptions on the happy
/// path. `Result<void, E>` is supported.

#include <cassert>
#include <new>
#include <type_traits>
#include <utility>
#include <variant>

#include "core/error.hpp"

namespace pvpgn::core {

template <class E>
class Failure {
public:
    explicit Failure(E e) : error_(std::move(e)) {}
    const E& error() const& noexcept { return error_; }
    E&&      error()      && noexcept { return std::move(error_); }

private:
    E error_;
};

template <class E>
Failure<std::decay_t<E>> fail(E&& e) {
    return Failure<std::decay_t<E>>{std::forward<E>(e)};
}

template <class T, class E = Error>
class [[nodiscard]] Result {
    static_assert(!std::is_same_v<std::decay_t<T>, std::decay_t<E>>,
                  "Result<T,E> with T == E is not supported");

public:
    using value_type = T;
    using error_type = E;

    Result(T v) : data_(std::in_place_index<0>, std::move(v)) {}             // NOLINT
    Result(Failure<E> f) : data_(std::in_place_index<1>, std::move(f).error()) {}  // NOLINT

    [[nodiscard]] bool has_value() const noexcept { return data_.index() == 0; }
    [[nodiscard]] explicit operator bool() const noexcept { return has_value(); }

    T& value() & {
        assert(has_value());
        return std::get<0>(data_);
    }
    const T& value() const& {
        assert(has_value());
        return std::get<0>(data_);
    }
    T&& value() && {
        assert(has_value());
        return std::get<0>(std::move(data_));
    }

    // Member / dereference access to the contained value, mirroring
    // std::expected / std::optional. Precondition: has_value().
    T*       operator->() noexcept { assert(has_value()); return &std::get<0>(data_); }
    const T* operator->() const noexcept { assert(has_value()); return &std::get<0>(data_); }
    T&       operator*() & noexcept { assert(has_value()); return std::get<0>(data_); }
    const T& operator*() const& noexcept { assert(has_value()); return std::get<0>(data_); }

    const E& error() const& {
        assert(!has_value());
        return std::get<1>(data_);
    }
    E&& error() && {
        assert(!has_value());
        return std::get<1>(std::move(data_));
    }

    [[nodiscard]] T value_or(T fallback) const& {
        return has_value() ? std::get<0>(data_) : std::move(fallback);
    }

    /// fmap: f : T -> U  =>  Result<U,E>
    template <class F>
    auto map(F&& f) const& -> Result<std::invoke_result_t<F, const T&>, E> {
        using U = std::invoke_result_t<F, const T&>;
        if (has_value())
            return Result<U, E>{std::forward<F>(f)(value())};
        return Result<U, E>{fail(error())};
    }

    /// monadic bind: f : T -> Result<U,E>
    template <class F>
    auto and_then(F&& f) const& -> std::invoke_result_t<F, const T&> {
        using R = std::invoke_result_t<F, const T&>;
        static_assert(std::is_same_v<typename R::error_type, E>,
                      "and_then: result error type must match");
        if (has_value()) return std::forward<F>(f)(value());
        return R{fail(error())};
    }

    /// fmap on error.
    template <class F>
    auto map_error(F&& f) const& -> Result<T, std::invoke_result_t<F, const E&>> {
        using E2 = std::invoke_result_t<F, const E&>;
        if (has_value()) return Result<T, E2>{value()};
        return Result<T, E2>{fail(std::forward<F>(f)(error()))};
    }

private:
    std::variant<T, E> data_;
};

/// Specialisation for `Result<void, E>` — common for fire-and-forget ops.
template <class E>
class [[nodiscard]] Result<void, E> {
public:
    using value_type = void;
    using error_type = E;

    Result() noexcept = default;
    Result(Failure<E> f) : error_(std::move(f).error()), ok_(false) {}  // NOLINT

    [[nodiscard]] bool has_value() const noexcept { return ok_; }
    [[nodiscard]] explicit operator bool() const noexcept { return ok_; }

    const E& error() const& {
        assert(!ok_);
        return error_;
    }
    E&& error() && {
        assert(!ok_);
        return std::move(error_);
    }

    template <class F>
    auto and_then(F&& f) const& -> std::invoke_result_t<F> {
        using R = std::invoke_result_t<F>;
        if (ok_) return std::forward<F>(f)();
        return R{fail(error_)};
    }

private:
    E    error_{};
    bool ok_ = true;
};

/// Convenience alias for the common `Result<T, Error>`.
template <class T = void>
using Status = Result<T, Error>;

/// nodiscard convention
/// ------------------------
/// `Result<T,E>` is `[[nodiscard]]` at the class level (see above), so every
/// function that returns a `Result` or `Status` already triggers a warning
/// when the result is silently dropped. Project policy: if a return value
/// must be ignored deliberately (e.g. best-effort logging on shutdown), use
/// an explicit `(void)expr;` -- never disable the warning globally.
///
/// In addition, observer/query members (`has_value`, `value_or`, `error`,
/// `Error::is_ok`, `Error::code`, etc.) are `[[nodiscard]]` because calling
/// them for their side effects is always a bug.

[[nodiscard]] inline Status<> ok() {
    return Status<>{};
}

}  // namespace pvpgn::core
