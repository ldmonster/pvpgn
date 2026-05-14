// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `LegacyBnetFrameRouter` -- the skeleton subclass of
// `LegacyProtocolHandler` that, once the linked-variant hook lands
// in batch 38b, will deliver each framed BNet packet to legacy
// `handle_bnet_packet`.
//
// Batch 38a (this file): no live legacy dispatch. The tests assert
// that:
//   1. Without an installed hook, frames are recorded.
//   2. With a hook that accepts, frames are NOT recorded.
//   3. With a hook that declines, frames ARE recorded.
//   4. The opaque connection pointer is forwarded verbatim.

#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "core/bytes.hpp"
#include "integration/legacy_bnetd/legacy_bnet_frame_router.hpp"
#include "integration/legacy_bnetd/legacy_protocol_handler.hpp"

using namespace pvpgn;
using integration::legacy_bnetd::ConnectionClass;
using integration::legacy_bnetd::LegacyBnetConnection;
using integration::legacy_bnetd::LegacyBnetFrameRouter;

namespace {

class NullEgress : public application::ports::IConnectionEgress {
public:
    void send(std::vector<std::byte>) override {}
    void close() override {}
};

// Stand-in for a real `t_connection*`. The router treats it as
// opaque so any non-null pointer works.
struct FakeConn {
    int marker;
};

void feed(LegacyBnetFrameRouter& r, std::initializer_list<std::uint8_t> raw) {
    std::vector<std::byte> b(raw.size());
    std::size_t i = 0;
    for (auto v : raw) b[i++] = std::byte{v};
    r.on_bytes(core::ByteView{b.data(), b.size()});
}

// RAII guard so a failing test does not leak the hook into the
// next case.
struct HookGuard {
    ~HookGuard() { LegacyBnetFrameRouter::clear_dispatch_hook(); }
};

}  // namespace

TEST_CASE("LegacyBnetFrameRouter records frames when no hook is installed",
          "[integration][legacy][router]") {
    HookGuard guard;
    LegacyBnetFrameRouter::clear_dispatch_hook();

    FakeConn conn{42};
    NullEgress eg;
    LegacyBnetFrameRouter r(reinterpret_cast<LegacyBnetConnection*>(&conn));
    r.start(eg);

    // BNet frame: type=0xFF, sid=0x44, size=8 LE, 4 body bytes.
    feed(r, {0xFF, 0x44, 0x08, 0x00, 0x01, 0x02, 0x03, 0x04});
    auto recorded = r.drain_recorded();
    REQUIRE(recorded.size() == 1);
    REQUIRE(recorded[0].payload.size() == 8);
    REQUIRE(static_cast<std::uint8_t>(recorded[0].payload[1]) == 0x44);
}

TEST_CASE("LegacyBnetFrameRouter forwards to installed hook and records nothing",
          "[integration][legacy][router]") {
    HookGuard guard;

    FakeConn conn{7};
    NullEgress eg;
    LegacyBnetFrameRouter r(reinterpret_cast<LegacyBnetConnection*>(&conn));
    r.start(eg);

    std::vector<std::size_t> seen_sizes;
    LegacyBnetConnection*    seen_conn = nullptr;
    LegacyBnetFrameRouter::set_dispatch_hook(
        [&](LegacyBnetConnection* c,
            std::span<const std::byte> bytes,
            application::ports::IConnectionEgress&) {
            seen_conn = c;
            seen_sizes.push_back(bytes.size());
            return true;  // claim ownership
        });

    feed(r, {0xFF, 0x44, 0x08, 0x00, 0x01, 0x02, 0x03, 0x04});
    feed(r, {0xFF, 0x50, 0x06, 0x00, 0xAA, 0xBB});

    REQUIRE(seen_sizes == std::vector<std::size_t>{8, 6});
    REQUIRE(seen_conn == reinterpret_cast<LegacyBnetConnection*>(&conn));
    REQUIRE(r.drain_recorded().empty());
}

TEST_CASE("LegacyBnetFrameRouter falls back to recording when hook declines",
          "[integration][legacy][router]") {
    HookGuard guard;

    FakeConn conn{1};
    NullEgress eg;
    LegacyBnetFrameRouter r(reinterpret_cast<LegacyBnetConnection*>(&conn));
    r.start(eg);

    LegacyBnetFrameRouter::set_dispatch_hook(
        [](LegacyBnetConnection*,
           std::span<const std::byte>,
           application::ports::IConnectionEgress&) {
            return false;  // decline
        });

    feed(r, {0xFF, 0x44, 0x08, 0x00, 0x01, 0x02, 0x03, 0x04});
    auto recorded = r.drain_recorded();
    REQUIRE(recorded.size() == 1);
    REQUIRE(recorded[0].payload.size() == 8);
}
