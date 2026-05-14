// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file legacy_bnet_frame_router.hpp
/// `LegacyBnetFrameRouter` -- a `LegacyProtocolHandler` subclass that
/// is intended (in batch 38b) to deliver each framed BNet packet to
/// the legacy `pvpgn::bnetd::handle_bnet_packet` and drain the
/// connection outqueue back through the v3 egress port.
///
/// **Status (batch 38a -- this commit): skeleton only.**
/// `dispatch_frame()` consults a process-global hook
/// (`set_legacy_dispatch_hook`) and falls back to recording the
/// frame for tests when no hook is installed. The hook itself is
/// installed by an opaque TU in the linked variant in batch 38b.
///
/// The legacy `t_connection*` is supplied by the caller -- whoever
/// constructs the router decides how to build it. In batch 38c this
/// will be `TcpBridge`'s accept handler in the gated rollout path.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <utility>
#include <vector>

#include "integration/legacy_bnetd/legacy_protocol_handler.hpp"

namespace pvpgn::integration::legacy_bnetd {

/// Forward-declared so this header has no dependency on the legacy
/// `bnetd/connection.h`. The pointer is opaque to v3.
struct LegacyBnetConnection;

class LegacyBnetFrameRouter final : public LegacyProtocolHandler {
public:
    /// Signature of the dispatch hook. Receives the opaque
    /// connection pointer, the framed payload (including the
    /// 4-byte bnet header), and the v3 egress port for outbound
    /// bytes. The hook is responsible for pushing any reply
    /// packets through `egress` (typically by draining the legacy
    /// outqueue after `handle_bnet_packet` returns). Returns
    /// `true` if the hook handled the frame; on `false` the
    /// router falls back to recording the frame for tests.
    using DispatchHook = std::function<bool(LegacyBnetConnection*,
                                            std::span<const std::byte>,
                                            application::ports::IConnectionEgress&)>;

    /// Constructs a router for an already-built `t_connection*`.
    /// `connection` MUST be non-null and outlive the router.
    /// `cls` defaults to `Bnet` since that's the only class wired
    /// through this router today; once the bnet `init` byte has
    /// already been consumed by the caller, frames come in as
    /// length-prefixed bnet packets.
    explicit LegacyBnetFrameRouter(LegacyBnetConnection* connection,
                                   ConnectionClass cls = ConnectionClass::Bnet) noexcept
        : LegacyProtocolHandler(cls), conn_(connection) {}

    LegacyBnetConnection* connection() const noexcept { return conn_; }

    /// Synchronously forward an outbound packet's raw bytes through
    /// the router's egress port. Used by the linked-variant TU
    /// `conn_push_outqueue` redirect path (38c) so packets pushed
    /// asynchronously by legacy code (timers, channel broadcasts,
    /// `/whisper` reply etc.) reach the v3 socket instead of the
    /// fdwatch writer. Returns `false` if `start()` has not yet
    /// been called.
    bool send_outbound(std::span<const std::byte> bytes) noexcept;

    /// Install the global dispatch hook. The linked variant
    /// (`legacy_bnet_frame_router_link.cpp`, batch 38b) calls this
    /// from a static-init struct so the hook is live as soon as
    /// `bnetd_legacy` is in the binary. Tests can also install a
    /// stub hook to assert what the router would forward.
    static void set_dispatch_hook(DispatchHook hook) noexcept;
    static void clear_dispatch_hook() noexcept;

    /// Signature of the class-refresh hook (38f). After
    /// `dispatch_via_legacy` runs `handle_init_packet`, the legacy
    /// connection transitions from `conn_class_init` to
    /// `conn_class_bnet`. The router consults this hook after each
    /// successful dispatch to mirror that transition on the v3 side
    /// so subsequent frames are parsed with the right framing.
    /// Returning the same class is a no-op.
    using ClassRefresh = std::function<ConnectionClass(LegacyBnetConnection*)>;
    static void set_class_refresh(ClassRefresh hook) noexcept;
    static void clear_class_refresh() noexcept;

    /// For tests: drain the frames that arrived while no hook was
    /// installed (or while the hook returned `false`).
    std::vector<LegacyFrame> drain_recorded() noexcept {
        return std::exchange(recorded_, {});
    }

protected:
    void dispatch_frame(LegacyFrame frame) override;

private:
    LegacyBnetConnection*    conn_;
    std::vector<LegacyFrame> recorded_;
};

}  // namespace pvpgn::integration::legacy_bnetd
