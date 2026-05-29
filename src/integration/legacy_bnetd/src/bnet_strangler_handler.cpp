// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/bnet_strangler_handler.hpp"

#include <utility>
#include <variant>
#include <vector>

#include "application/ports/connection_handler.hpp"
#include "core/bytes.hpp"
#include "core/error.hpp"
#include "protocol/bnet/anongame.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/common/packet.hpp"
#include "protocol/common/writer.hpp"

namespace pvpgn::integration::legacy_bnetd {

// ---- EgressContext ------------------------------------------------------

core::Status<>
BnetStranglerHandler::EgressContext::send(
    const protocol::bnet::ServerMessage& msg) {
    auto* eg = owner_.egress();
    if (!eg) {
        return core::fail(core::make_error(
            core::StatusCode::FailedPrecondition,
            "bnet strangler: no egress configured"));
    }
    protocol::Writer w(64);
    auto status = std::visit(
        [&w](const auto& m) -> core::Status<> {
            return protocol::bnet::encode(w, m);
        },
        msg);
    if (!status) return status;
    eg->send(w.take());
    return core::ok();
}

void BnetStranglerHandler::EgressContext::close() {
    if (auto* eg = owner_.egress()) eg->close();
}

// ---- BnetStranglerHandler ----------------------------------------------

void BnetStranglerHandler::dispatch_frame(LegacyFrame frame) {
    // Non-BNCS classes go straight to the legacy fallback. The
    // strangler only owns the BNCS opcode space.
    if (frame.cls != ConnectionClass::Bnet) {
        to_fallback(std::move(frame));
        return;
    }

    // Parse the 4-byte BNCS header so we can decide on the opcode
    // before paying for a full decode. This mirrors what the legacy
    // dispatch loop does — we are *not* re-framing here; the parent
    // class already gave us a complete frame.
    const core::ByteView buf{frame.payload.data(), frame.payload.size()};
    auto hdr = protocol::parse_bnet_header(buf);
    if (!hdr) {
        // Malformed header — let legacy log + close in its usual way.
        to_fallback(std::move(frame));
        return;
    }
    if (allowed_sids_.find(hdr.value().code) == allowed_sids_.end()) {
        to_fallback(std::move(frame));
        return;
    }

    // Opcode is on the allow-list — decode and drive the FSM. If
    // either step fails we still fall back to legacy so behaviour is
    // unchanged for clients exercising edge cases the v3 path hasn't
    // grown for yet.
    auto packet = protocol::parse_packet(buf);
    if (!packet) {
        to_fallback(std::move(frame));
        return;
    }
    auto decoded = protocol::bnet::decode_client(packet.value().packet);
    if (!decoded) {
        to_fallback(std::move(frame));
        return;
    }

    // Special-case the FINDANONGAME (0x44) sub-option multiplexer:
    // route INFOREQ packets through the v3 INFOREPLY pipeline; defer
    // every other sub-option to the legacy handler.
    if (hdr.value().code == 0x44 /* kSidWarcraftGeneral */) {
        auto* env = std::get_if<protocol::bnet::WarcraftGeneralRequest>(
            &decoded.value());
        if (!env || !inforeply_resolver_) {
            to_fallback(std::move(frame));
            return;
        }
        auto sub = protocol::bnet::parse_findanongame_request(*env);
        if (!sub) {
            to_fallback(std::move(frame));
            return;
        }
        auto* req = std::get_if<protocol::bnet::AnonGameInfoRequest>(
            &sub.value());
        if (!req) {
            // Other 0x44 sub-options (search/cancel/profile/...): legacy.
            to_fallback(std::move(frame));
            return;
        }
        auto bytes = inforeply_resolver_(*req);
        if (!bytes) {
            to_fallback(std::move(frame));
            return;
        }
        if (auto* eg = egress(); eg && !bytes.value().empty()) {
            eg->send(std::move(bytes).value());
        }
        ++handled_v3_;
        ++inforeply_n_;
        return;
    }

    auto fsm_status = fsm_.handle(decoded.value());
    if (!fsm_status) {
        // FSM rejected the message (illegal in current state). Let
        // legacy take over — it may have a more permissive policy
        // for pre-handshake noise, and it owns the close decision.
        to_fallback(std::move(frame));
        return;
    }

    ++handled_v3_;
}

void BnetStranglerHandler::to_fallback(LegacyFrame frame) {
    ++fallback_n_;
    if (fallback_) fallback_(std::move(frame));
}

}  // namespace pvpgn::integration::legacy_bnetd
