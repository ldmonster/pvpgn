// SPDX-License-Identifier: GPL-2.0-or-later
#include <cstddef>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "protocol/bnet/codec.hpp"
#include "protocol/common/replay.hpp"
#include "protocol/common/writer.hpp"

using namespace pvpgn;

namespace {

// Build a captured byte stream of N packets: Null, Ping(0x11), Null.
std::vector<std::byte> golden_stream() {
    protocol::Writer w;
    (void)protocol::bnet::encode(w, protocol::bnet::Null{});
    (void)protocol::bnet::encode(w, protocol::bnet::Ping{0x00000011u});
    (void)protocol::bnet::encode(w, protocol::bnet::Null{});
    return w.take();
}

}  // namespace

TEST_CASE("replay: full stream decodes to N messages",
          "[protocol][replay]") {
    auto bytes = golden_stream();
    auto r = protocol::replay<protocol::bnet::ClientMessage>(
        core::ByteView{bytes}, protocol::bnet::decode_client);
    REQUIRE(r.has_value());
    REQUIRE(r.value().stats.packets_decoded == 3);
    REQUIRE(r.value().stats.bytes_consumed  == bytes.size());
    REQUIRE(r.value().stats.bytes_trailing  == 0);
    REQUIRE(r.value().messages.size() == 3);
    REQUIRE(std::holds_alternative<protocol::bnet::Null>(r.value().messages[0]));
    REQUIRE(std::get<protocol::bnet::Ping>(r.value().messages[1]).ticks == 0x11u);
    REQUIRE(std::holds_alternative<protocol::bnet::Null>(r.value().messages[2]));
}

TEST_CASE("replay: partial tail surfaced via stats.bytes_trailing",
          "[protocol][replay]") {
    auto bytes = golden_stream();
    // Truncate last 2 bytes — last Null packet (4B header) becomes partial.
    core::ByteView truncated{bytes.data(), bytes.size() - 2};
    auto r = protocol::replay<protocol::bnet::ClientMessage>(
        truncated, protocol::bnet::decode_client);
    REQUIRE(r.has_value());
    REQUIRE(r.value().stats.packets_decoded == 2);
    REQUIRE(r.value().stats.bytes_trailing  == 2);
    REQUIRE(r.value().messages.size() == 2);
}

TEST_CASE("replay: hard error propagates",
          "[protocol][replay]") {
    // Single packet with unknown SID code 0xAB.
    std::array<std::byte, 4> raw{
        std::byte{0xFF}, std::byte{0xAB}, std::byte{0x04}, std::byte{0x00}};
    auto r = protocol::replay<protocol::bnet::ClientMessage>(
        core::ByteView{raw}, protocol::bnet::decode_client);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::Unimplemented);
}

TEST_CASE("replay: empty stream returns zero packets",
          "[protocol][replay]") {
    auto r = protocol::replay<protocol::bnet::ClientMessage>(
        core::ByteView{}, protocol::bnet::decode_client);
    REQUIRE(r.has_value());
    REQUIRE(r.value().stats.packets_decoded == 0);
    REQUIRE(r.value().messages.empty());
}
