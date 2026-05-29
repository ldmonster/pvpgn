// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file secret.hpp
/// `core::Secret<T>` — a wrapper that prevents accidental logging of
/// sensitive values (passwords, shared keys, tokens, …).
///
/// Design
/// ──────
/// • `operator<<` always prints `"***"` so that structured-logging
///   frameworks, fmt, and plain `std::ostream` never leak the value.
/// • `reveal()` is the only way to obtain the underlying value; the
///   name is intentionally loud so that code-review catches misuse.
/// • For `Secret<std::string>` the destructor overwrites the string
///   storage with zeros before clearing, reducing the window during
///   which the secret lives in heap memory.
/// • `from_string()` supports three resolution modes:
///     - `env:<VAR>`   — read the named environment variable
///     - `file:<path>` — read the first non-empty line of the file
///     - anything else — use the literal value as-is
///
/// Round 331: initial implementation.

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

namespace pvpgn::core {

template <typename T>
class Secret {
public:
    /// Construct from a value.
    explicit Secret(T value) : value_(std::move(value)) {}

    /// Destructor — for `std::string` specialisation below, the storage
    /// is zeroed.  For other types the default destructor is sufficient.
    ~Secret() = default;

    // Non-copyable to avoid accidental duplication of secrets.
    Secret(const Secret&)            = delete;
    Secret& operator=(const Secret&) = delete;

    // Movable.
    Secret(Secret&&) noexcept            = default;
    Secret& operator=(Secret&&) noexcept = default;

    /// Return the underlying value.  The name is intentionally verbose
    /// so that every call site is visible during code review.
    [[nodiscard]] const T& reveal() const noexcept { return value_; }

    /// Always prints `"***"` — never the actual value.
    friend std::ostream& operator<<(std::ostream& os, const Secret& /*s*/)
    {
        return os << "***";
    }

private:
    T value_;
};

// ── Specialisation for std::string ───────────────────────────────────────────

template <>
class Secret<std::string> {
public:
    explicit Secret(std::string value) : value_(std::move(value)) {}

    /// Zeroise the string storage before destruction.
    ~Secret()
    {
        if (!value_.empty()) {
            // Overwrite every character with zero to reduce the window
            // during which the secret lives in heap memory.
            std::fill(value_.begin(), value_.end(), '\0');
            value_.clear();
        }
    }

    Secret(const Secret&)            = delete;
    Secret& operator=(const Secret&) = delete;

    Secret(Secret&&) noexcept            = default;
    Secret& operator=(Secret&&) noexcept = default;

    [[nodiscard]] const std::string& reveal() const noexcept { return value_; }

    friend std::ostream& operator<<(std::ostream& os, const Secret& /*s*/)
    {
        return os << "***";
    }

    // ── Resolution factory ────────────────────────────────────────────────────

    /// Resolve a raw string into a `Secret<std::string>`.
    ///
    /// Resolution rules (checked in order):
    ///   1. Prefix `env:`  — read the named environment variable.
    ///      If the variable is unset the secret is an empty string.
    ///   2. Prefix `file:` — open the file and return the first
    ///      non-empty line (leading/trailing whitespace stripped).
    ///      If the file cannot be opened the secret is an empty string.
    ///   3. Otherwise      — use `raw` as the literal value.
    static Secret<std::string> from_string(std::string_view raw)
    {
        constexpr std::string_view kEnvPrefix  = "env:";
        constexpr std::string_view kFilePrefix = "file:";

        if (raw.size() >= kEnvPrefix.size() &&
            raw.substr(0, kEnvPrefix.size()) == kEnvPrefix) {
            const std::string var_name{raw.substr(kEnvPrefix.size())};
            // NOLINTNEXTLINE(concurrency-mt-unsafe) — read-only env access
            const char* val = std::getenv(var_name.c_str());
            return Secret<std::string>{val ? std::string{val} : std::string{}};
        }

        if (raw.size() >= kFilePrefix.size() &&
            raw.substr(0, kFilePrefix.size()) == kFilePrefix) {
            const std::string path{raw.substr(kFilePrefix.size())};
            std::ifstream ifs{path};
            if (ifs.is_open()) {
                std::string line;
                while (std::getline(ifs, line)) {
                    // Strip leading/trailing whitespace.
                    const auto first = line.find_first_not_of(" \t\r\n");
                    if (first == std::string::npos) continue;
                    const auto last = line.find_last_not_of(" \t\r\n");
                    return Secret<std::string>{line.substr(first, last - first + 1)};
                }
            }
            return Secret<std::string>{std::string{}};
        }

        return Secret<std::string>{std::string{raw}};
    }

private:
    std::string value_;
};

}  // namespace pvpgn::core
