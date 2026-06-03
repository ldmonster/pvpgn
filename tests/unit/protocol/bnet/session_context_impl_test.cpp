// SPDX-License-Identifier: GPL-2.0-or-later
//
// tests/unit/protocol/bnet/session_context_impl_test.cpp
//
// Unit coverage for the *real* `BnetSessionContextImpl` — the adapter that
// encodes a ServerMessage and pushes the framed bytes to an IConnectionEgress.
//
// Why this exists: every FSM test uses `CapturingSessionContext`, a full mock
// of `ISessionContext` that records `ServerMessage` objects *before* encoding.
// That left the production `send()` path — encode → finalize → egress — with
// zero coverage, which is exactly how this regression slipped in:
//
//   send() called finalize_bnet_packet() a SECOND time, but each encode()
//   already finalizes its own packet, so the redundant call returned
//   FailedPrecondition and send() bailed *before* egress_->send(). No
//   ServerMessage ever reached the wire. (Found via the e2e login journey;
//   fixed by dropping the redundant finalize.)
//
// These tests assert the observable contract: one send() → exactly one
// well-framed packet on the egress, byte-for-byte round-trippable back to the
// original message. A reintroduction of the double-finalize bug makes the
// egress receive nothing and fails the very first assertion.

#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "domain/connection/ports.hpp"
#include "domain/shared/ids.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/bnet/session_context_impl.hpp"
#include "protocol/common/packet.hpp"

using namespace pvpgn;
using namespace pvpgn::protocol;
using namespace pvpgn::protocol::bnet;

namespace {

/// Test double for IConnectionEgress: records every buffer handed to send().
class CapturingEgress final : public domain::connection::IConnectionEgress {
public:
    void send(std::vector<std::byte> bytes) override {
        buffers.push_back(std::move(bytes));
    }
    void close() override { closed = true; }

    std::vector<std::vector<std::byte>> buffers;
    bool                                closed = false;
};

}  // namespace

TEST_CASE("BnetSessionContextImpl::send pushes exactly one framed packet",
          "[protocol][bnet][session]") {
    auto egress = std::make_shared<CapturingEgress>();
    BnetSessionContextImpl ctx{domain::SessionId{1}, egress};

    auto st = ctx.send(ServerMessage{AuthCheckReply{0u, ""}});
    REQUIRE(st.has_value());                       // send succeeded
    REQUIRE(egress->buffers.size() == 1);          // regression: was 0 with the bug
    REQUIRE_FALSE(egress->buffers[0].empty());
}

TEST_CASE("BnetSessionContextImpl::send frames AuthCheckReply correctly",
          "[protocol][bnet][session]") {
    auto egress = std::make_shared<CapturingEgress>();
    BnetSessionContextImpl ctx{domain::SessionId{7}, egress};

    REQUIRE(ctx.send(ServerMessage{AuthCheckReply{0u, ""}}).has_value());
    REQUIRE(egress->buffers.size() == 1);
    const auto& buf = egress->buffers[0];

    // Header: marker 0xFF, code = SID_AUTH_CHECK (0x51), size == buffer length.
    auto hdr = parse_bnet_header(core::ByteView{buf.data(), buf.size()});
    REQUIRE(hdr.has_value());
    CHECK(hdr.value().marker == 0xFFu);
    CHECK(hdr.value().code == kSidAuthCheck);
    CHECK(hdr.value().size == buf.size());

    // Full round-trip: the framed bytes decode back to the original message.
    auto fp = parse_packet(core::ByteView{buf.data(), buf.size()});
    REQUIRE(fp.has_value());
    auto msg = decode_server(fp.value().packet);
    REQUIRE(msg.has_value());
    auto* reply = std::get_if<AuthCheckReply>(&msg.value());
    REQUIRE(reply != nullptr);
    CHECK(*reply == AuthCheckReply{0u, ""});
}

TEST_CASE("BnetSessionContextImpl::send frames LogonResponse2Reply (reject)",
          "[protocol][bnet][session]") {
    auto egress = std::make_shared<CapturingEgress>();
    BnetSessionContextImpl ctx{domain::SessionId{3}, egress};

    // 0x01 = account does not exist; no reason string on the wire.
    REQUIRE(ctx.send(ServerMessage{LogonResponse2Reply{0x01u, ""}}).has_value());
    REQUIRE(egress->buffers.size() == 1);
    const auto& buf = egress->buffers[0];

    auto hdr = parse_bnet_header(core::ByteView{buf.data(), buf.size()});
    REQUIRE(hdr.has_value());
    CHECK(hdr.value().code == kSidLogonResponse2);
    CHECK(hdr.value().size == buf.size());

    auto fp = parse_packet(core::ByteView{buf.data(), buf.size()});
    REQUIRE(fp.has_value());
    auto msg = decode_server(fp.value().packet);
    REQUIRE(msg.has_value());
    auto* reply = std::get_if<LogonResponse2Reply>(&msg.value());
    REQUIRE(reply != nullptr);
    CHECK(reply->result == 0x01u);
}

TEST_CASE("BnetSessionContextImpl::send queues each message independently",
          "[protocol][bnet][session]") {
    auto egress = std::make_shared<CapturingEgress>();
    BnetSessionContextImpl ctx{domain::SessionId{9}, egress};

    REQUIRE(ctx.send(ServerMessage{AuthCheckReply{0u, ""}}).has_value());
    REQUIRE(ctx.send(ServerMessage{LogonResponse2Reply{0x00u, ""}}).has_value());

    // Two sends → two distinct framed packets, in order.
    REQUIRE(egress->buffers.size() == 2);
    auto h0 = parse_bnet_header(
        core::ByteView{egress->buffers[0].data(), egress->buffers[0].size()});
    auto h1 = parse_bnet_header(
        core::ByteView{egress->buffers[1].data(), egress->buffers[1].size()});
    REQUIRE(h0.has_value());
    REQUIRE(h1.has_value());
    CHECK(h0.value().code == kSidAuthCheck);
    CHECK(h1.value().code == kSidLogonResponse2);
}

TEST_CASE("BnetSessionContextImpl::close forwards to the egress",
          "[protocol][bnet][session]") {
    auto egress = std::make_shared<CapturingEgress>();
    BnetSessionContextImpl ctx{domain::SessionId{2}, egress};

    CHECK_FALSE(egress->closed);
    ctx.close();
    CHECK(egress->closed);
}

TEST_CASE("BnetSessionContextImpl::send fails cleanly with no egress",
          "[protocol][bnet][session]") {
    BnetSessionContextImpl ctx{domain::SessionId{5}, nullptr};
    auto st = ctx.send(ServerMessage{AuthCheckReply{0u, ""}});
    CHECK_FALSE(st.has_value());   // Internal error, no crash
}
