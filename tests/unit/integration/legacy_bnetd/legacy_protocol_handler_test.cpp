// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for the v3 strangler-fig adapter `LegacyProtocolHandler`.
// Exercises framing only (the seam-only milestone): no legacy
// `handle_*_packet` is invoked yet.

#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "core/bytes.hpp"
#include "integration/legacy_bnetd/legacy_protocol_handler.hpp"

using namespace pvpgn;
using integration::legacy_bnetd::ConnectionClass;
using integration::legacy_bnetd::LegacyFrame;
using integration::legacy_bnetd::LegacyProtocolHandler;

namespace {

class StubEgress : public application::ports::IConnectionEgress {
public:
    void send(std::vector<std::byte> bytes) override {
        sent.push_back(std::move(bytes));
    }
    void close() override { closed = true; }
    std::vector<std::vector<std::byte>> sent;
    bool                                closed = false;
};

std::vector<std::byte> bytes_of(std::string_view s) {
    std::vector<std::byte> out(s.size());
    std::memcpy(out.data(), s.data(), s.size());
    return out;
}

void feed(LegacyProtocolHandler& h, std::initializer_list<std::uint8_t> raw) {
    std::vector<std::byte> b(raw.size());
    std::size_t i = 0;
    for (auto v : raw) b[i++] = std::byte{v};
    h.on_bytes(core::ByteView{b.data(), b.size()});
}

}  // namespace

TEST_CASE("LegacyProtocolHandler: Init class consumes one byte per frame",
          "[integration][legacy][framing]") {
    StubEgress eg;
    LegacyProtocolHandler h(ConnectionClass::Init);
    h.start(eg);

    feed(h, {0x01, 0x02, 0x03});
    auto frames = h.drain_dispatched();
    REQUIRE(frames.size() == 3);
    REQUIRE(frames[0].cls == ConnectionClass::Init);
    REQUIRE(static_cast<std::uint8_t>(frames[0].payload[0]) == 0x01);
    REQUIRE(static_cast<std::uint8_t>(frames[2].payload[0]) == 0x03);
}

TEST_CASE("LegacyProtocolHandler: Bnet class needs full LE-prefixed frame",
          "[integration][legacy][framing]") {
    StubEgress eg;
    LegacyProtocolHandler h(ConnectionClass::Bnet);
    h.start(eg);

    // type=0x00FF, size=8 (LE), 4 bytes of body
    feed(h, {0xFF, 0x00, 0x08, 0x00, 0xAA, 0xBB, 0xCC, 0xDD});
    auto frames = h.drain_dispatched();
    REQUIRE(frames.size() == 1);
    REQUIRE(frames[0].payload.size() == 8);
}

TEST_CASE("LegacyProtocolHandler: Bnet stitches frame across reads",
          "[integration][legacy][framing]") {
    StubEgress eg;
    LegacyProtocolHandler h(ConnectionClass::Bnet);
    h.start(eg);

    feed(h, {0xFF, 0x00, 0x08, 0x00});  // header only
    REQUIRE(h.drain_dispatched().empty());
    feed(h, {0xAA, 0xBB});                // partial body
    REQUIRE(h.drain_dispatched().empty());
    feed(h, {0xCC, 0xDD});                // rest of body
    auto frames = h.drain_dispatched();
    REQUIRE(frames.size() == 1);
    REQUIRE(frames[0].payload.size() == 8);
}

TEST_CASE("LegacyProtocolHandler: Bnet rejects oversized declared size",
          "[integration][legacy][framing][error]") {
    StubEgress eg;
    LegacyProtocolHandler h(ConnectionClass::Bnet);
    h.start(eg);

    // Declared size 0xFFFF > MAX_PACKET_SIZE (3072). Buffer drops.
    feed(h, {0x00, 0x00, 0xFF, 0xFF, 0xAA});
    REQUIRE(h.drain_dispatched().empty());
    // Subsequent valid frame succeeds.
    feed(h, {0xFF, 0x00, 0x04, 0x00});
    auto frames = h.drain_dispatched();
    REQUIRE(frames.size() == 1);
}

TEST_CASE("LegacyProtocolHandler: WolGameres uses big-endian size prefix",
          "[integration][legacy][framing]") {
    StubEgress eg;
    LegacyProtocolHandler h(ConnectionClass::WolGameres);
    h.start(eg);

    // size BE = 6, then 4 bytes body
    feed(h, {0x00, 0x06, 0xDE, 0xAD, 0xBE, 0xEF});
    auto frames = h.drain_dispatched();
    REQUIRE(frames.size() == 1);
    REQUIRE(frames[0].payload.size() == 6);
}

TEST_CASE("LegacyProtocolHandler: Telnet frames split on newline",
          "[integration][legacy][framing][line]") {
    StubEgress eg;
    LegacyProtocolHandler h(ConnectionClass::Telnet);
    h.start(eg);

    auto buf = bytes_of("hello\nworld\nno-eol");
    h.on_bytes(core::ByteView{buf.data(), buf.size()});

    auto frames = h.drain_dispatched();
    REQUIRE(frames.size() == 2);
    REQUIRE(frames[0].payload.size() == 6);  // "hello\n"
    REQUIRE(frames[1].payload.size() == 6);  // "world\n"

    // The trailing "no-eol" stays buffered until newline arrives.
    auto more = bytes_of("\n");
    h.on_bytes(core::ByteView{more.data(), more.size()});
    auto more_frames = h.drain_dispatched();
    REQUIRE(more_frames.size() == 1);
    REQUIRE(more_frames[0].payload.size() == 7);  // "no-eol\n"
}

TEST_CASE("LegacyProtocolHandler: set_class switches mid-stream after Init",
          "[integration][legacy][framing][switch]") {
    StubEgress eg;
    LegacyProtocolHandler h(ConnectionClass::Init);
    h.start(eg);

    // First byte is the magic; in real life the owner reads it and
    // switches the handler. We do the same here.
    feed(h, {0x01});  // CLIENT_INITCONN_CLASS_BNET
    REQUIRE(h.drain_dispatched().size() == 1);
    h.set_class(ConnectionClass::Bnet);
    feed(h, {0xFF, 0x00, 0x05, 0x00, 0x42});
    auto frames = h.drain_dispatched();
    REQUIRE(frames.size() == 1);
    REQUIRE(frames[0].cls == ConnectionClass::Bnet);
    REQUIRE(frames[0].payload.size() == 5);
}

TEST_CASE("LegacyProtocolHandler: on_close stops further dispatch",
          "[integration][legacy][framing][lifecycle]") {
    StubEgress eg;
    LegacyProtocolHandler h(ConnectionClass::Bnet);
    h.start(eg);
    h.on_close();
    feed(h, {0xFF, 0x00, 0x04, 0x00});
    REQUIRE(h.drain_dispatched().empty());
}
