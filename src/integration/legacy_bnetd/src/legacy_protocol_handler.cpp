// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/legacy_protocol_handler.hpp"

#include <algorithm>
#include <cstring>

namespace pvpgn::integration::legacy_bnetd {

namespace {

constexpr std::size_t kBnetHeaderSize       = 4;  // u16 type LE + u16 size LE
constexpr std::size_t kLe16PrefixHeaderSize = 4;  // 2-byte size + 2-byte type
constexpr std::size_t kBe16PrefixHeaderSize = 2;
constexpr std::size_t kInitPacketSize       = 1;
constexpr std::size_t kMaxFrameSize         = 3072;  // legacy MAX_PACKET_SIZE

inline std::uint16_t read_le16(const std::byte* p) noexcept {
    return static_cast<std::uint16_t>(
        static_cast<std::uint8_t>(p[0])
        | (static_cast<std::uint16_t>(static_cast<std::uint8_t>(p[1])) << 8));
}

inline std::uint16_t read_be16(const std::byte* p) noexcept {
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(static_cast<std::uint8_t>(p[0])) << 8)
        | static_cast<std::uint8_t>(p[1]));
}

}  // namespace

void LegacyProtocolHandler::on_bytes(core::ByteView bytes) {
    if (closed_) return;
    rx_.insert(rx_.end(), bytes.begin(), bytes.end());
    while (!rx_.empty()) {
        const std::size_t consumed = try_consume_one_frame();
        if (consumed == 0) return;             // need more bytes
        if (consumed > rx_.size()) {           // defensive
            rx_.clear();
            return;
        }
        // Materialise the frame and hand it off.
        LegacyFrame f{
            cls_,
            std::vector<std::byte>(rx_.begin(),
                                   rx_.begin() + static_cast<std::ptrdiff_t>(consumed))};
        rx_.erase(rx_.begin(),
                  rx_.begin() + static_cast<std::ptrdiff_t>(consumed));
        dispatch_frame(std::move(f));
        if (closed_) return;
    }
}

void LegacyProtocolHandler::on_close() {
    closed_ = true;
}

std::size_t LegacyProtocolHandler::try_consume_one_frame() {
    switch (cls_) {
        case ConnectionClass::Init:
            return rx_.size() >= kInitPacketSize ? kInitPacketSize : 0;
        case ConnectionClass::Bnet:
            return bnet_style_frame_size();
        case ConnectionClass::File:
        case ConnectionClass::D2csBnetd:
        case ConnectionClass::W3route:
            return le16_prefixed_frame_size();
        case ConnectionClass::WolGameres:
            return be16_prefixed_frame_size();
        case ConnectionClass::Bot:
        case ConnectionClass::Telnet:
        case ConnectionClass::Irc:
        case ConnectionClass::Wol:
        case ConnectionClass::Wladder:
            return line_terminated_frame_size();
    }
    return 0;
}

std::size_t LegacyProtocolHandler::bnet_style_frame_size() const noexcept {
    if (rx_.size() < kBnetHeaderSize) return 0;
    // Legacy `t_bnet_header` is u16 type (LE) + u16 size (LE); `size`
    // is the total frame size including the header.
    const std::uint16_t total = read_le16(&rx_[2]);
    if (total < kBnetHeaderSize || total > kMaxFrameSize) {
        // Malformed — drop everything and resync. Tests can detect
        // by observing that no frame is emitted.
        const_cast<LegacyProtocolHandler*>(this)->rx_.clear();
        return 0;
    }
    return rx_.size() >= total ? total : 0;
}

std::size_t LegacyProtocolHandler::le16_prefixed_frame_size() const noexcept {
    if (rx_.size() < kLe16PrefixHeaderSize) return 0;
    // Legacy `t_file_header` / `t_w3route_header` / `t_d2cs_bnetd_header`
    // all start with a u16 LE size at offset 0 covering the whole frame.
    const std::uint16_t total = read_le16(&rx_[0]);
    if (total < kLe16PrefixHeaderSize || total > kMaxFrameSize) {
        const_cast<LegacyProtocolHandler*>(this)->rx_.clear();
        return 0;
    }
    return rx_.size() >= total ? total : 0;
}

std::size_t LegacyProtocolHandler::be16_prefixed_frame_size() const noexcept {
    if (rx_.size() < kBe16PrefixHeaderSize) return 0;
    const std::uint16_t total = read_be16(&rx_[0]);
    if (total < kBe16PrefixHeaderSize || total > kMaxFrameSize) {
        const_cast<LegacyProtocolHandler*>(this)->rx_.clear();
        return 0;
    }
    return rx_.size() >= total ? total : 0;
}

std::size_t LegacyProtocolHandler::line_terminated_frame_size() const noexcept {
    // Legacy bot/irc/telnet code reads 1 byte at a time and stops at
    // `\n`; the framing seen by the handler always **includes** the
    // trailing newline. We do the same here, but in batch.
    auto it = std::find(rx_.begin(), rx_.end(), std::byte{'\n'});
    if (it == rx_.end()) {
        if (rx_.size() >= kMaxFrameSize) {
            // Overflow without newline — flush the lot.
            return rx_.size();
        }
        return 0;
    }
    return static_cast<std::size_t>(it - rx_.begin()) + 1;
}

}  // namespace pvpgn::integration::legacy_bnetd
