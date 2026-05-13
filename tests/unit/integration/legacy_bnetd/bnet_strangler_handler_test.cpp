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
