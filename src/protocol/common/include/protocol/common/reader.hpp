// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file reader.hpp
/// Bounded, span-based reader over a packet payload. Every read checks
/// remaining bytes; on overflow a `core::Error{OutOfRange, ...}` is
/// returned and the cursor is **not** advanced.
///
/// Semantics:
///   * All multi-byte integers are little-endian (BNet wire format).
///     Use `read_be<T>()` when reading mixed-endian protocols.
///   * `read_cstring()` returns a `string_view` that aliases the
///     reader's buffer; lifetime is the buffer's.
///   * Reader is non-throwing; designed for use in fuzz harnesses.

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>

#include "core/bytes.hpp"
#include "core/endian.hpp"
#include "core/error.hpp"
#include "core/result.hpp"

namespace pvpgn::protocol {

class Reader {
public:
    explicit Reader(core::ByteView buf) noexcept : buf_(buf) {}

    std::size_t size()      const noexcept { return buf_.size(); }
    std::size_t position()  const noexcept { return pos_; }
    std::size_t remaining() const noexcept { return buf_.size() - pos_; }
    bool        empty()     const noexcept { return pos_ >= buf_.size(); }

    core::ByteView tail() const noexcept { return buf_.subspan(pos_); }

    /// Skip `n` bytes. OutOfRange if not enough remain.
    core::Status<> skip(std::size_t n) noexcept {
        if (remaining() < n) {
            return core::fail(core::make_error(
                core::StatusCode::OutOfRange, "reader: skip past end"));
        }
        pos_ += n;
        return core::ok();
    }

    /// Read a little-endian integer and advance.
    template <class T>
    core::Result<T> read_le() noexcept {
        static_assert(std::is_integral_v<T>);
        if (remaining() < sizeof(T)) return short_();
        auto v = core::read_le<T>(buf_.subspan(pos_, sizeof(T)));
        if (!v) return v;
        pos_ += sizeof(T);
        return v;
    }

    /// Read a big-endian integer and advance.
    template <class T>
    core::Result<T> read_be() noexcept {
        static_assert(std::is_integral_v<T>);
        if (remaining() < sizeof(T)) return short_();
        auto v = core::read_be<T>(buf_.subspan(pos_, sizeof(T)));
        if (!v) return v;
        pos_ += sizeof(T);
        return v;
    }

    /// Read exactly `n` raw bytes (no decoding) and advance.
    core::Result<core::ByteView> read_bytes(std::size_t n) noexcept {
        if (remaining() < n) return short_();
        auto out = buf_.subspan(pos_, n);
        pos_ += n;
        return out;
    }

    /// Read a NUL-terminated string and advance past the terminator.
    /// Returns a view that does **not** include the NUL.
    core::Result<std::string_view> read_cstring() noexcept {
        for (std::size_t i = pos_; i < buf_.size(); ++i) {
            if (static_cast<std::uint8_t>(buf_[i]) == 0u) {
                const auto* start =
                    reinterpret_cast<const char*>(buf_.data() + pos_);
                std::string_view s(start, i - pos_);
                pos_ = i + 1;
                return s;
            }
        }
        return core::fail(core::make_error(
            core::StatusCode::OutOfRange, "reader: unterminated string"));
    }

private:
    static core::Failure<core::Error> short_() noexcept {
        return core::fail(core::make_error(
            core::StatusCode::OutOfRange, "reader: short buffer"));
    }

    core::ByteView buf_;
    std::size_t    pos_ = 0;
};

}  // namespace pvpgn::protocol
