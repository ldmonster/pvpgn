// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file bnet_strangler_handler.hpp
/// First protocol-level strangler-fig cut on the BNCS wire.
///
/// `BnetStranglerHandler` is a `LegacyProtocolHandler` subclass that
/// intercepts framed `ConnectionClass::Bnet` packets and, for an
/// allow-listed set of SIDs, routes them through the pure v3
/// protocol layer (`protocol::bnet::decode_client` +
/// `protocol::bnet::BnetFsm`). Everything else — unknown SIDs,
/// decode failures, FSM rejects, non-Bnet classes — is handed to a
/// caller-supplied fallback (typically the existing legacy
/// `handle_*_packet` path).
///
/// **Scope of this first cut.**
/// Only `SID_NULL` (0x00, keepalive, no payload, no reply) is
/// handled in v3. The point is to prove the seam works end-to-end:
///   * legacy framing → v3 codec → v3 FSM → ack-or-fallback,
///   * fallback for anything not in the allow-list, with no behaviour
///     change for the rest of the protocol,
///   * unit-testable without standing up the legacy server.
///
/// Adding a new opcode to the v3 side is then a matter of
///   1. extending the codec / FSM, and
///   2. adding the SID to `allowed_sids_`.
///
/// Replies from the FSM are encoded via `protocol::Writer` and
/// pushed back out through the v3 `IConnectionEgress`.

#include <cstdint>
#include <cstddef>
#include <functional>
#include <unordered_set>
#include <utility>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"
#include "integration/legacy_bnetd/legacy_protocol_handler.hpp"
#include "protocol/bnet/anongame.hpp"
#include "protocol/bnet/fsm.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/bnet/session_context.hpp"

namespace pvpgn::integration::legacy_bnetd {

/// Callback invoked when the strangler decides a frame must go to
/// the legacy path. Receives ownership of the frame so the
/// implementation can move bytes into a `t_packet` without copying.
using OnLegacyFallback = std::function<void(LegacyFrame)>;

/// Callback that turns an `AnonGameInfoRequest` into a ready-to-send
/// SID 0x44 byte stream (typically by selecting a per-locale
/// `CompiledSnapshot` and calling
/// `application::anongame_infoply::encode_inforeplies_for_request`).
/// Returning a failure status causes the strangler to fall back to
/// the legacy handler for that frame.
using AnonGameInforeplyResolver = std::function<
    core::Result<std::vector<std::byte>>(
        const protocol::bnet::AnonGameInfoRequest&)>;

class BnetStranglerHandler final : public LegacyProtocolHandler {
public:
    /// @param cls Initial connection class. For a bnet listener this
    ///   is usually `Init` and gets switched to `Bnet` by the caller
    ///   after the magic-byte preface.
    /// @param fallback Where to send anything we don't handle in v3.
    ///   If unset, unhandled frames are silently dropped — useful for
    ///   tests but not for production.
    explicit BnetStranglerHandler(ConnectionClass cls,
                                   OnLegacyFallback fallback = {})
        : LegacyProtocolHandler(cls),
          fallback_(std::move(fallback)),
          ctx_(*this),
          fsm_(std::make_shared<EgressContext>(*this), protocol::bnet::BnetUseCaseContext{}) {
        // Currently the v3 layer is authoritative for SID_NULL only.
        // Add more opcodes here as the corresponding FSM/codec arms
        // graduate from "stub" to "feature-complete".
        allowed_sids_.insert(0x00 /* kSidNull */);
    }

    /// For tests + observability: number of frames handled in v3 vs
    /// handed to the fallback. Updated from the network thread only.
    std::size_t handled_v3_count()  const noexcept { return handled_v3_; }
    std::size_t fallback_count()    const noexcept { return fallback_n_; }
    std::size_t inforeply_count()   const noexcept { return inforeply_n_; }

    /// Install the FINDANONGAME (SID 0x44) INFOREPLY pipeline. When
    /// set, well-formed INFOREQ packets are answered in v3 by calling
    /// `resolver`, which is expected to return the full concatenated
    /// SID 0x44 byte stream of all INFOREPLY packets. Other 0x44
    /// sub-options still fall back to legacy. Calling this with an
    /// empty `resolver` removes the hook and reverts 0x44 to fallback.
    void set_anongame_inforeply_resolver(AnonGameInforeplyResolver resolver) {
        inforeply_resolver_ = std::move(resolver);
        if (inforeply_resolver_) {
            allowed_sids_.insert(0x44 /* kSidWarcraftGeneral */);
        } else {
            allowed_sids_.erase(0x44);
        }
    }

    /// Expose the FSM state for tests; never mutate it from outside.
    protocol::bnet::BnetState fsm_state() const noexcept {
        return fsm_.state();
    }

protected:
    void dispatch_frame(LegacyFrame frame) override;

private:
    /// `ISessionContext` adapter that pushes encoded server messages
    /// out through the owning handler's `IConnectionEgress`.
    class EgressContext final : public protocol::bnet::ISessionContext {
    public:
        explicit EgressContext(BnetStranglerHandler& owner) noexcept
            : owner_(owner) {}
        core::Status<> send(const protocol::bnet::ServerMessage& msg) override;
        void           close() override;
    private:
        BnetStranglerHandler& owner_;
    };

    void to_fallback(LegacyFrame frame);

    OnLegacyFallback                fallback_;
    AnonGameInforeplyResolver       inforeply_resolver_;
    std::unordered_set<std::uint8_t> allowed_sids_;
    EgressContext                    ctx_;
    protocol::bnet::BnetFsm          fsm_;
    std::size_t                      handled_v3_  = 0;
    std::size_t                      fallback_n_  = 0;
    std::size_t                      inforeply_n_ = 0;
};

}  // namespace pvpgn::integration::legacy_bnetd
