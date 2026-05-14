// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `BnetStranglerHandler` — the first protocol-level
// strangler cut routing SID_NULL through the v3 codec+FSM and
// everything else to a legacy fallback.

#include <cstring>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "application/ports/connection_handler.hpp"
#include "core/bytes.hpp"
#include "integration/legacy_bnetd/bnet_strangler_handler.hpp"
#include "integration/legacy_bnetd/legacy_protocol_handler.hpp"
#include "protocol/bnet/anongame.hpp"

using namespace pvpgn;
using integration::legacy_bnetd::BnetStranglerHandler;
using integration::legacy_bnetd::ConnectionClass;
using integration::legacy_bnetd::LegacyFrame;

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

void feed(BnetStranglerHandler& h, std::initializer_list<std::uint8_t> raw) {
    std::vector<std::byte> b(raw.size());
    std::size_t i = 0;
    for (auto v : raw) b[i++] = std::byte{v};
    h.on_bytes(core::ByteView{b.data(), b.size()});
}

}  // namespace

TEST_CASE("BnetStrangler: SID_NULL (keepalive) is handled in v3, no fallback",
          "[integration][strangler][bnet]") {
    StubEgress eg;
    std::vector<LegacyFrame> seen_legacy;
    BnetStranglerHandler h(ConnectionClass::Bnet,
        [&](LegacyFrame f) { seen_legacy.push_back(std::move(f)); });
    h.start(eg);

    // 0xFF 0x00 0x04 0x00 — BNCS NULL, no body.
    feed(h, {0xFF, 0x00, 0x04, 0x00});

    REQUIRE(h.handled_v3_count() == 1);
    REQUIRE(h.fallback_count()   == 0);
    REQUIRE(seen_legacy.empty());
    REQUIRE(eg.sent.empty());          // NULL has no reply
    REQUIRE_FALSE(eg.closed);
}

TEST_CASE("BnetStrangler: two back-to-back NULLs both go through v3",
          "[integration][strangler][bnet]") {
    StubEgress eg;
    std::vector<LegacyFrame> seen_legacy;
    BnetStranglerHandler h(ConnectionClass::Bnet,
        [&](LegacyFrame f) { seen_legacy.push_back(std::move(f)); });
    h.start(eg);

    feed(h, {0xFF, 0x00, 0x04, 0x00, 0xFF, 0x00, 0x04, 0x00});

    REQUIRE(h.handled_v3_count() == 2);
    REQUIRE(seen_legacy.empty());
}

TEST_CASE("BnetStrangler: unknown SID falls back to legacy",
          "[integration][strangler][bnet][fallback]") {
    StubEgress eg;
    std::vector<LegacyFrame> seen_legacy;
    BnetStranglerHandler h(ConnectionClass::Bnet,
        [&](LegacyFrame f) { seen_legacy.push_back(std::move(f)); });
    h.start(eg);

    // 0x42 is not on the v3 allow-list.
    feed(h, {0xFF, 0x42, 0x04, 0x00});

    REQUIRE(h.handled_v3_count() == 0);
    REQUIRE(h.fallback_count()   == 1);
    REQUIRE(seen_legacy.size()   == 1);
    REQUIRE(seen_legacy[0].cls == ConnectionClass::Bnet);
    REQUIRE(seen_legacy[0].payload.size() == 4);
    REQUIRE(static_cast<std::uint8_t>(seen_legacy[0].payload[1]) == 0x42);
}

TEST_CASE("BnetStrangler: NULL on a non-Bnet class always falls back",
          "[integration][strangler][bnet][fallback]") {
    StubEgress eg;
    std::vector<LegacyFrame> seen_legacy;
    // Telnet uses line framing. We feed a line so the parent class
    // emits one frame; the strangler must hand it to the fallback
    // because cls != Bnet — the v3 BNCS path has no opinion on
    // telnet bytes.
    BnetStranglerHandler h(ConnectionClass::Telnet,
        [&](LegacyFrame f) { seen_legacy.push_back(std::move(f)); });
    h.start(eg);

    feed(h, {'o', 'p', '\n'});

    REQUIRE(h.handled_v3_count() == 0);
    REQUIRE(h.fallback_count()   == 1);
    REQUIRE(seen_legacy.size()   == 1);
    REQUIRE(seen_legacy[0].cls == ConnectionClass::Telnet);
}

TEST_CASE("BnetStrangler: malformed bnet header (bad marker) falls back",
          "[integration][strangler][bnet][fallback]") {
    StubEgress eg;
    std::vector<LegacyFrame> seen_legacy;
    BnetStranglerHandler h(ConnectionClass::Bnet,
        [&](LegacyFrame f) { seen_legacy.push_back(std::move(f)); });
    h.start(eg);

    // Marker 0xFE is illegal (must be 0xFF) — the framer doesn't
    // validate the marker, it only uses the size field, so we still
    // get a complete frame here. Strangler must route to fallback so
    // legacy decides what to do (typically: close).
    feed(h, {0xFE, 0x00, 0x04, 0x00});

    REQUIRE(h.handled_v3_count() == 0);
    REQUIRE(h.fallback_count()   == 1);
    REQUIRE(seen_legacy.size()   == 1);
}

TEST_CASE("BnetStrangler: missing fallback drops unhandled frames silently",
          "[integration][strangler][bnet][drop]") {
    StubEgress eg;
    // No fallback callback — handler must not crash and must still
    // increment the counter.
    BnetStranglerHandler h(ConnectionClass::Bnet);
    h.start(eg);

    feed(h, {0xFF, 0x42, 0x04, 0x00});

    REQUIRE(h.handled_v3_count() == 0);
    REQUIRE(h.fallback_count()   == 1);
}

// =========================================================================
// FINDANONGAME (SID 0x44) INFOREQ -> v3 INFOREPLY pipeline hook
// =========================================================================
//
// We don't pull in the full INFOREPLY service stack here: the hook is
// just a `std::function` callback. The test verifies the strangler
// (a) decodes the wire envelope correctly, (b) calls the callback with
// the typed `AnonGameInfoRequest`, (c) writes the callback's bytes
// back through the egress, and (d) falls back when the callback isn't
// installed.

namespace {

// Build the SID-0x44 wire bytes for an INFOREQ with one entry.
std::vector<std::uint8_t> make_inforeq_bytes(
    std::uint32_t count, std::uint32_t tag, std::uint32_t tag_unk) {
    // Body = sub_option(1) + count(4) + noitems(1) + entry(8) = 14 bytes
    // Packet = 0xFF 0x44 len_lo len_hi body = 18 bytes total
    auto u8 = [](std::uint32_t v) {
        return static_cast<std::uint8_t>(v & 0xFFu);
    };
    std::vector<std::uint8_t> p;
    p.reserve(18);
    p.push_back(0xFF);
    p.push_back(0x44);
    p.push_back(18);    // len_lo
    p.push_back(0);     // len_hi
    p.push_back(0x02);  // sub_option = kAnonGameClientInfos (INFOREQ)
    p.push_back(u8(count));
    p.push_back(u8(count >> 8));
    p.push_back(u8(count >> 16));
    p.push_back(u8(count >> 24));
    p.push_back(0x01);  // noitems
    p.push_back(u8(tag));
    p.push_back(u8(tag >> 8));
    p.push_back(u8(tag >> 16));
    p.push_back(u8(tag >> 24));
    p.push_back(u8(tag_unk));
    p.push_back(u8(tag_unk >> 8));
    p.push_back(u8(tag_unk >> 16));
    p.push_back(u8(tag_unk >> 24));
    return p;
}

void feed_bytes(BnetStranglerHandler& h, const std::vector<std::uint8_t>& raw) {
    std::vector<std::byte> b(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) b[i] = std::byte{raw[i]};
    h.on_bytes(core::ByteView{b.data(), b.size()});
}

}  // namespace

TEST_CASE("BnetStrangler: SID 0x44 INFOREQ routes to v3 inforeply resolver",
          "[integration][strangler][bnet][anongame]") {
    StubEgress eg;
    std::vector<LegacyFrame> seen_legacy;
    BnetStranglerHandler h(ConnectionClass::Bnet,
        [&](LegacyFrame f) { seen_legacy.push_back(std::move(f)); });

    protocol::bnet::AnonGameInfoRequest captured;
    bool resolver_called = false;
    h.set_anongame_inforeply_resolver(
        [&](const protocol::bnet::AnonGameInfoRequest& req)
            -> core::Result<std::vector<std::byte>> {
            captured = req;
            resolver_called = true;
            return std::vector<std::byte>{
                std::byte{0xFF}, std::byte{0x44},
                std::byte{0xAA}, std::byte{0xBB},
            };
        });
    h.start(eg);

    feed_bytes(h, make_inforeq_bytes(0xDEADBEEFu, 0x4C5255u /*'URL\0'*/, 0u));

    REQUIRE(resolver_called);
    REQUIRE(captured.count == 0xDEADBEEFu);
    REQUIRE(captured.noitems == 1);
    REQUIRE(captured.entries.size() == 1);
    REQUIRE(captured.entries[0].tag == 0x4C5255u);

    REQUIRE(h.handled_v3_count() == 1);
    REQUIRE(h.inforeply_count()  == 1);
    REQUIRE(h.fallback_count()   == 0);
    REQUIRE(seen_legacy.empty());
    REQUIRE(eg.sent.size() == 1);
    REQUIRE(eg.sent[0].size() == 4);
}

TEST_CASE("BnetStrangler: SID 0x44 falls back when no resolver is set",
          "[integration][strangler][bnet][anongame][fallback]") {
    StubEgress eg;
    std::vector<LegacyFrame> seen_legacy;
    BnetStranglerHandler h(ConnectionClass::Bnet,
        [&](LegacyFrame f) { seen_legacy.push_back(std::move(f)); });
    h.start(eg);

    feed_bytes(h, make_inforeq_bytes(1u, 0x4C5255u, 0u));

    REQUIRE(h.handled_v3_count() == 0);
    REQUIRE(h.inforeply_count()  == 0);
    REQUIRE(h.fallback_count()   == 1);
    REQUIRE(seen_legacy.size()   == 1);
}

TEST_CASE("BnetStrangler: SID 0x44 non-INFOREQ sub-options always fall back",
          "[integration][strangler][bnet][anongame][fallback]") {
    StubEgress eg;
    std::vector<LegacyFrame> seen_legacy;
    BnetStranglerHandler h(ConnectionClass::Bnet,
        [&](LegacyFrame f) { seen_legacy.push_back(std::move(f)); });

    bool resolver_called = false;
    h.set_anongame_inforeply_resolver(
        [&](const protocol::bnet::AnonGameInfoRequest&)
            -> core::Result<std::vector<std::byte>> {
            resolver_called = true;
            return std::vector<std::byte>{};
        });
    h.start(eg);

    // sub_option = 0x07 (CLIENT_TOURNAMENT) with 4-byte count body.
    std::vector<std::uint8_t> raw{
        0xFF, 0x44, 9, 0,        // header (len = 9)
        0x07,                    // sub_option != INFOREQ
        0x01, 0x00, 0x00, 0x00,  // count
    };
    feed_bytes(h, raw);

    REQUIRE_FALSE(resolver_called);
    REQUIRE(h.handled_v3_count() == 0);
    REQUIRE(h.inforeply_count()  == 0);
    REQUIRE(h.fallback_count()   == 1);
    REQUIRE(seen_legacy.size()   == 1);
}

TEST_CASE("BnetStrangler: resolver returning failure triggers legacy fallback",
          "[integration][strangler][bnet][anongame][fallback]") {
    StubEgress eg;
    std::vector<LegacyFrame> seen_legacy;
    BnetStranglerHandler h(ConnectionClass::Bnet,
        [&](LegacyFrame f) { seen_legacy.push_back(std::move(f)); });

    h.set_anongame_inforeply_resolver(
        [&](const protocol::bnet::AnonGameInfoRequest&)
            -> core::Result<std::vector<std::byte>> {
            return core::fail(core::make_error(
                core::StatusCode::Internal, "synthetic"));
        });
    h.start(eg);

    feed_bytes(h, make_inforeq_bytes(1u, 0x4C5255u, 0u));

    REQUIRE(h.handled_v3_count() == 0);
    REQUIRE(h.inforeply_count()  == 0);
    REQUIRE(h.fallback_count()   == 1);
    REQUIRE(seen_legacy.size()   == 1);
    REQUIRE(eg.sent.empty());
}

TEST_CASE("BnetStrangler: removing the resolver reverts 0x44 to fallback",
          "[integration][strangler][bnet][anongame]") {
    StubEgress eg;
    std::vector<LegacyFrame> seen_legacy;
    BnetStranglerHandler h(ConnectionClass::Bnet,
        [&](LegacyFrame f) { seen_legacy.push_back(std::move(f)); });

    h.set_anongame_inforeply_resolver(
        [&](const protocol::bnet::AnonGameInfoRequest&)
            -> core::Result<std::vector<std::byte>> {
            return std::vector<std::byte>{std::byte{0x00}};
        });
    h.start(eg);

    feed_bytes(h, make_inforeq_bytes(1u, 0x4C5255u, 0u));
    REQUIRE(h.inforeply_count() == 1);

    // Remove the hook; subsequent 0x44 frames must now fall back.
    h.set_anongame_inforeply_resolver({});
    feed_bytes(h, make_inforeq_bytes(2u, 0x4C5255u, 0u));

    REQUIRE(h.inforeply_count() == 1);  // unchanged
    REQUIRE(h.fallback_count()  == 1);
    REQUIRE(seen_legacy.size()  == 1);
}
