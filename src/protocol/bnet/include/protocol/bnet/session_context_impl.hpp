// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file session_context_impl.hpp
/// Concrete implementation of ISessionContext for BNet.
/// Encodes ServerMessage using Writer and sends via IConnectionEgress.
/// Holds a SessionId for logging/tracking.

#include <memory>

#include "domain/connection/ports.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/bnet/session_context.hpp"
#include "protocol/common/writer.hpp"

namespace pvpgn::protocol::bnet {

class BnetSessionContextImpl final : public ISessionContext {
public:
    BnetSessionContextImpl(
        domain::SessionId session_id,
        std::shared_ptr<domain::connection::IConnectionEgress> egress)
        : session_id_(session_id), egress_(egress) {}

    core::Status<> send(const ServerMessage& msg) override {
        if (!egress_) {
            return core::fail(core::make_error(core::StatusCode::Internal,
                                                "egress not available"));
        }

        // Encode the message using a Writer. Each `encode()` overload opens
        // *and* finalizes its own BNet packet (begin_bnet_packet → write →
        // finalize_bnet_packet), so the buffer is already a complete framed
        // packet here. Do NOT call finalize_bnet_packet() again: there is no
        // open packet, so it would return FailedPrecondition and silently
        // suppress the send (the bug that left BnetFsm replies un-sent).
        Writer w;
        auto result = std::visit(
            [&w](const auto& m) { return encode(w, m); }, msg);
        if (!result) {
            return result;  // Propagate encode error
        }

        // Send via egress (non-blocking queue)
        egress_->send(w.take());
        return core::ok();
    }

    void close() override {
        if (egress_) {
            egress_->close();
        }
    }

    domain::SessionId session_id() const noexcept { return session_id_; }

private:
    domain::SessionId session_id_;
    std::shared_ptr<domain::connection::IConnectionEgress> egress_;
};

}  // namespace pvpgn::protocol::bnet
