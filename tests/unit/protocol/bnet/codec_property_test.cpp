// SPDX-License-Identifier: GPL-2.0-or-later
//
// tests/unit/protocol/bnet/codec_property_test.cpp -- Plan 10 property tests.
//
// In-tree property-based tests for the bnet codec (no rapidcheck dependency):
//   1. Round-trip identity — for a generated message, encode → frame →
//      decode_client yields an equal message (decode is a left-inverse of
//      encode).
//   2. Decode robustness — decode_client / decode_server on arbitrary
//      well-framed packets always terminates and returns a Result (never
//      crashes / reads out of bounds). Under the ASan/UBSan CI jobs this same
//      test is a sanitizer target for the decoders.
//
// A fixed-seed std::mt19937 makes failures reproducible; each case runs many
// iterations to approximate property coverage.

#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "protocol/bnet/codec.hpp"
#include "protocol/common/packet.hpp"
#include "protocol/common/writer.hpp"

using namespace pvpgn;

namespace {

// Deterministic generator (fixed seed → reproducible failures).
struct Gen {
    std::mt19937 rng{0xC0DEC0DEu};

    std::uint32_t u32() { return static_cast<std::uint32_t>(rng()); }
    std::uint8_t  u8() { return static_cast<std::uint8_t>(rng()); }

    // A NUL-free printable string (round-trip identity requires no embedded
    // NUL, since the wire format is NUL-terminated).
    std::string text(std::size_t max_len = 40) {
        const std::size_t n = rng() % (max_len + 1);
        std::string s;
        s.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            s.push_back(static_cast<char>(0x21 + (rng() % (0x7E - 0x21))));
        }
        return s;
    }

    // Arbitrary bytes (may contain NUL) — for the robustness fuzz property.
    std::vector<std::byte> bytes(std::size_t max_len) {
        const std::size_t n = rng() % (max_len + 1);
        std::vector<std::byte> v(n);
        for (auto& b : v) b = static_cast<std::byte>(rng());
        return v;
    }
};

// encode a message, frame it, and decode it back through the real wire path.
template <class Msg, class DecodeFn>
auto round_trip(const Msg& m, DecodeFn decode) {
    protocol::Writer w;
    REQUIRE(protocol::bnet::encode(w, m).has_value());
    auto fp = protocol::parse_packet(w.view());
    REQUIRE(fp.has_value());
    return decode(fp.value().packet);
}

}  // namespace

TEST_CASE("bnet codec property: Ping round-trips for any cookie",
          "[protocol][bnet][property]") {
    using namespace protocol::bnet;
    Gen g;
    for (int i = 0; i < 4000; ++i) {
        Ping in{g.u32()};
        INFO("iteration " << i << " ticks=" << in.ticks);
        auto r = round_trip(in, decode_client);
        REQUIRE(r.has_value());
        REQUIRE(std::holds_alternative<Ping>(r.value()));
        REQUIRE(std::get<Ping>(r.value()) == in);
    }
}

TEST_CASE("bnet codec property: JoinChannel round-trips",
          "[protocol][bnet][property]") {
    using namespace protocol::bnet;
    Gen g;
    for (int i = 0; i < 2000; ++i) {
        JoinChannel in{g.u32(), g.text()};
        INFO("iteration " << i << " channel='" << in.channel << "'");
        auto r = round_trip(in, decode_client);
        REQUIRE(r.has_value());
        REQUIRE(std::holds_alternative<JoinChannel>(r.value()));
        REQUIRE(std::get<JoinChannel>(r.value()) == in);
    }
}

TEST_CASE("bnet codec property: ChatCommand round-trips",
          "[protocol][bnet][property]") {
    using namespace protocol::bnet;
    Gen g;
    for (int i = 0; i < 2000; ++i) {
        ChatCommand in{g.text(120)};
        INFO("iteration " << i << " text='" << in.text << "'");
        auto r = round_trip(in, decode_client);
        REQUIRE(r.has_value());
        REQUIRE(std::holds_alternative<ChatCommand>(r.value()));
        REQUIRE(std::get<ChatCommand>(r.value()) == in);
    }
}

TEST_CASE("bnet codec property: decode never crashes on arbitrary packets",
          "[protocol][bnet][property]") {
    using namespace protocol::bnet;
    Gen g;
    int decoded_ok = 0;
    for (int i = 0; i < 6000; ++i) {
        // Build a well-framed packet: 0xFF magic, random SID, LE16 total size,
        // then a random body. This passes the framing layer so the per-SID
        // decoders are exercised on arbitrary bodies.
        auto body = g.bytes(80);
        const std::size_t total = 4 + body.size();
        std::vector<std::byte> buf;
        buf.reserve(total);
        buf.push_back(std::byte{0xFF});
        buf.push_back(static_cast<std::byte>(g.u8()));  // SID
        buf.push_back(static_cast<std::byte>(total & 0xFF));
        buf.push_back(static_cast<std::byte>((total >> 8) & 0xFF));
        buf.insert(buf.end(), body.begin(), body.end());

        auto fp = protocol::parse_packet(core::ByteView{buf.data(), buf.size()});
        if (!fp.has_value()) continue;  // framing rejected it — fine

        // The contract: decode returns a Result and never crashes / reads OOB.
        auto rc = decode_client(fp.value().packet);
        auto rs = decode_server(fp.value().packet);
        if (rc.has_value() || rs.has_value()) ++decoded_ok;
    }
    // Sanity: at least some random packets decoded (the SID space hits real
    // handlers), proving we actually exercised the decoders rather than only
    // bouncing off the framing layer.
    INFO("decoded_ok=" << decoded_ok);
    CHECK(decoded_ok > 0);
}
