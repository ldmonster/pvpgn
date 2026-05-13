// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file writer.hpp
/// Growable, owning byte writer used to build outbound packets.
///
/// The writer reserves room for a 4-byte BNet-style header up-front
/// and back-patches the total size in `finalize()`. Callers can also
/// use it as a generic LE/BE buffer for non-BNet protocols by skipping
/// `begin_bnet_packet()`.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/bytes.hpp"
#include "core/endian.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "protocol/common/packet.hpp"

namespace pvpgn::protocol {

class Writer {
public:
    Writer() = default;
    explicit Writer(std::size_t reserve) { buf_.reserve(reserve); }

    std::size_t size()  const noexcept { return buf_.size(); }
    bool        empty() const noexcept { return buf_.empty(); }

    core::ByteView view() const noexcept {
        return core::ByteView{buf_.data(), buf_.size()};
    }

    /// Take ownership of the underlying storage. Resets the writer.
    std::vector<std::byte> take() noexcept {
        std::vector<std::byte> out = std::move(buf_);
        buf_.clear();
        return out;
    }

    void clear() noexcept { buf_.clear(); }

    /// Append a single byte.
    void write_u8(std::uint8_t v) {
        buf_.push_back(static_cast<std::byte>(v));
    }

    template <class T>
    void write_le(T v) {
        static_assert(std::is_integral_v<T>);
        const auto off = buf_.size();
        buf_.resize(off + sizeof(T));
        (void)core::write_le<T>(core::ByteSpan{buf_.data() + off, sizeof(T)}, v);
    }

    template <class T>
    void write_be(T v) {
        static_assert(std::is_integral_v<T>);
        const auto off = buf_.size();
        buf_.resize(off + sizeof(T));
        (void)core::write_be<T>(core::ByteSpan{buf_.data() + off, sizeof(T)}, v);
    }

    /// Append raw bytes verbatim.
    void write_bytes(core::ByteView bytes) {
        buf_.insert(buf_.end(), bytes.begin(), bytes.end());
    }

    /// Append a string and a NUL terminator.
    void write_cstring(std::string_view s) {
        const auto off = buf_.size();
        buf_.resize(off + s.size() + 1);
        if (!s.empty()) {
            std::memcpy(buf_.data() + off, s.data(), s.size());
        }
        buf_[off + s.size()] = std::byte{0};
    }

    /// Begin a BNet packet: writes a header with the given code and size=0.
    /// The size field is back-patched in `finalize_bnet_packet()`.
    void begin_bnet_packet(std::uint8_t code) {
        bnet_start_ = buf_.size();
        BnetHeader hdr{kBnetMarker, code, 0};
        buf_.resize(buf_.size() + BnetHeader::kSize);
        (void)write_bnet_header(
            core::ByteSpan{buf_.data() + bnet_start_, BnetHeader::kSize}, hdr);
    }

    /// Back-patch the size field of the in-progress BNet packet.
    /// Returns `FailedPrecondition` if there is no open packet, or
    /// `OutOfRange` if the body exceeds `uint16_t`.
    core::Status<> finalize_bnet_packet() {
        if (bnet_start_ == kNoPacket) {
            return core::fail(core::make_error(
                core::StatusCode::FailedPrecondition,
                "writer: no open bnet packet"));
        }
        const std::size_t total = buf_.size() - bnet_start_;
        if (total > 0xFFFFu) {
            return core::fail(core::make_error(
                core::StatusCode::OutOfRange,
                "writer: packet exceeds uint16 size"));
        }
        BnetHeader hdr{kBnetMarker,
                       static_cast<std::uint8_t>(buf_[bnet_start_ + 1]),
                       static_cast<std::uint16_t>(total)};
        (void)write_bnet_header(
            core::ByteSpan{buf_.data() + bnet_start_, BnetHeader::kSize}, hdr);
        bnet_start_ = kNoPacket;
        return core::ok();
    }

private:
    static constexpr std::size_t kNoPacket =
        static_cast<std::size_t>(-1);

    std::vector<std::byte> buf_;
    std::size_t            bnet_start_ = kNoPacket;
};

}  // namespace pvpgn::protocol
