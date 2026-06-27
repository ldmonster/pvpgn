// SPDX-License-Identifier: GPL-2.0-or-later
//
// Unit tests for BnetFramer's stream-framing guards. The original server frames
// purely by the 16-bit size field and treats any packet whose declared size is
// below the 4-byte header or above MAX_PACKET_SIZE (3072) as corrupt, destroying
// the connection (packet_get_size() returns 0 -> total_size < header_size ->
// "corrupted packet received (closing connection)"). These tests pin that the
// framer requests a close on those sizes and frames valid sizes normally.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "main/bnet_framer.hpp"

namespace {

using pvpgn::app::bnetd::BnetFramer;

// Build a raw 4-byte BNet header (0xFF, sid, size-LE-u16) plus `body` filler so
// the buffer holds at least the header (and optionally a full declared packet).
std::vector<std::byte> header(std::uint8_t sid, std::uint16_t declared) {
    std::vector<std::byte> v;
    v.push_back(static_cast<std::byte>(0xFF));
    v.push_back(static_cast<std::byte>(sid));
    v.push_back(static_cast<std::byte>(declared & 0xFF));
    v.push_back(static_cast<std::byte>((declared >> 8) & 0xFF));
    return v;
}

// A no-op message sink; these tests only care about wants_close framing.
auto sink = [](auto&&) {};

}  // namespace

TEST_CASE("BnetFramer: declared size above MAX_PACKET_SIZE closes the connection",
          "[app][bnetd][framer]") {
    // Only the header is fed: the original closes right after the header,
    // before any body bytes arrive, so the guard must not wait for the body.
    for (std::uint16_t sz : {std::uint16_t(3073), std::uint16_t(4000),
                             std::uint16_t(60000), std::uint16_t(65535)}) {
        BnetFramer f;
        auto buf = header(0x70, sz);
        f.feed(pvpgn::core::ByteView{buf.data(), buf.size()}, sink);
        CHECK(f.wants_close);
    }
}

TEST_CASE("BnetFramer: declared size at MAX_PACKET_SIZE boundary does not close",
          "[app][bnetd][framer]") {
    // 3072 is the inclusive maximum; the connection must stay open (the body is
    // simply incomplete here, so the framer just awaits more bytes).
    BnetFramer f;
    auto buf = header(0x70, 3072);
    f.feed(pvpgn::core::ByteView{buf.data(), buf.size()}, sink);
    CHECK_FALSE(f.wants_close);
}

TEST_CASE("BnetFramer: declared size below header still closes (lower bound)",
          "[app][bnetd][framer]") {
    for (std::uint16_t sz : {std::uint16_t(0), std::uint16_t(1),
                             std::uint16_t(3)}) {
        BnetFramer f;
        auto buf = header(0x70, sz);
        f.feed(pvpgn::core::ByteView{buf.data(), buf.size()}, sink);
        CHECK(f.wants_close);
    }
}

TEST_CASE("BnetFramer: bad-marker header above MAX_PACKET_SIZE closes",
          "[app][bnetd][framer]") {
    // A non-0xFF leading byte yields an unmatched header; the original still
    // frames by size and treats >MAX_PACKET_SIZE as corrupt, so close (do not
    // attempt the size-based resync).
    BnetFramer f;
    auto buf = header(0x70, 4000);
    buf[0] = static_cast<std::byte>(0x00);  // corrupt the marker
    f.feed(pvpgn::core::ByteView{buf.data(), buf.size()}, sink);
    CHECK(f.wants_close);
}
