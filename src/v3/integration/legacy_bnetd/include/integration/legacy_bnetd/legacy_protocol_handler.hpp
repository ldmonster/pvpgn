// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file legacy_protocol_handler.hpp
/// `LegacyProtocolHandler` — the strangler-fig adapter that bridges
/// the v3 network layer to the legacy `handle_*_packet` family in
/// `src/bnetd/`.
///
/// Phase 2 of the migration plan (refactoring-plan-15 §"Network spine
/// on Asio + Fiber") asks for an adapter that:
///   1. accepts raw bytes from `infra::net::TcpSession`,
///   2. frames them per the legacy connection class
///      (length-prefixed for `bnet`/`init`/`d2cs_bnetd`/`file`/
///      `w3route`; line-terminated for `bot`/`telnet`/`irc`/`wol`/
///      `wserv`/`wladder`; fixed-size for `wolgameres`),
///   3. delivers each complete frame to the appropriate
///      `handle_*_packet` function with a freshly built `t_packet`,
///   4. drains the legacy outbound queue and forwards bytes via the
///      v3 egress port.
///
/// **Status (this commit): seam-only.**  No legacy linkage yet.
/// Steps 1–2 are implemented and unit-tested; steps 3–4 are stubbed
/// behind a virtual `dispatch_frame` hook so the framing logic can
/// be exercised in isolation.  Step 3/4 land in a follow-up
/// (refactor `src/bnetd/CMakeLists.txt` to export a `bnetd_legacy`
/// static library and link it here).

#include <cstdint>
#include <utility>
#include <vector>

#include "application/ports/connection_handler.hpp"
#include "core/bytes.hpp"

namespace pvpgn::integration::legacy_bnetd {

/// Mirrors `pvpgn::t_conn_class` from the legacy code-base. We keep
/// our own enum here so v3 doesn't transitively include the legacy
/// headers; the values do **not** need to match numerically.
enum class ConnectionClass : std::uint8_t {
    Init,         // legacy conn_class_init        — magic-byte preface
    Bnet,         // legacy conn_class_bnet        — length-prefixed
    File,         // legacy conn_class_file        — length-prefixed
    D2csBnetd,    // legacy conn_class_d2cs_bnetd  — length-prefixed
    W3route,      // legacy conn_class_w3route     — length-prefixed
    WolGameres,   // legacy conn_class_wgameres    — length-prefixed
    Bot,          // legacy conn_class_bot         — line-terminated
    Telnet,       // legacy conn_class_telnet      — line-terminated
    Irc,          // legacy conn_class_irc/_init   — line-terminated
    Wol,          // legacy conn_class_wol/_wserv  — line-terminated
    Wladder,      // legacy conn_class_wladder     — line-terminated
};

/// One framed packet, ready for legacy `handle_*_packet`.
/// `payload` always includes the protocol header (length, type, …)
/// when the protocol has one.
struct LegacyFrame {
    ConnectionClass        cls;
    std::vector<std::byte> payload;
};

class LegacyProtocolHandler : public application::ports::IConnectionHandler {
public:
    /// @param cls Connection class. Set by whoever decides the role
    ///   for this socket. For pure `bnet` listeners this is `Init`
    ///   until the first byte is read.
    explicit LegacyProtocolHandler(ConnectionClass cls) noexcept
        : cls_(cls) {}

    void start(application::ports::IConnectionEgress& out) override {
        out_ = &out;
    }

    void on_bytes(core::ByteView bytes) override;
    void on_close() override;

    /// Switch class mid-stream (used after the `init` magic byte
    /// reveals the real protocol).
    void set_class(ConnectionClass c) noexcept { cls_ = c; }
    ConnectionClass current_class() const noexcept { return cls_; }

protected:
    /// Hook invoked once a complete frame is buffered. The default
    /// implementation accumulates frames into `dispatched_` so unit
    /// tests can inspect them. The legacy-link subclass (delivered
    /// in a follow-up step) overrides this to call
    /// `handle_*_packet`.
    virtual void dispatch_frame(LegacyFrame frame) {
        dispatched_.push_back(std::move(frame));
    }

    /// Egress channel given by the network layer. Available after
    /// `start()` has been called. Subclasses use this to push
    /// outbound bytes (e.g. encoded v3 protocol replies).
    application::ports::IConnectionEgress* egress() const noexcept {
        return out_;
    }

public:
    /// For tests: drain the frames that the default `dispatch_frame`
    /// has accumulated.
    std::vector<LegacyFrame> drain_dispatched() {
        return std::exchange(dispatched_, {});
    }

private:
    /// Try to consume one frame from `rx_`. Returns the number of
    /// bytes consumed, or `0` if the buffer doesn't yet hold a full
    /// frame.
    std::size_t try_consume_one_frame();

    /// Length-prefixed framing: BNet-style 4-byte header (magic,
    /// type, len_lo, len_hi) — actually `len` is u16 at offset 2.
    /// Returns total frame size or `0` if header/body incomplete.
    std::size_t bnet_style_frame_size() const noexcept;

    /// File / W3route / D2csBnetd: 2-byte little-endian size at
    /// offset 0 (covers header + body).
    std::size_t le16_prefixed_frame_size() const noexcept;

    /// WolGameres: 2-byte big-endian size at offset 0.
    std::size_t be16_prefixed_frame_size() const noexcept;

    /// Line-terminated: scans `rx_` for `\n`. Returns position after
    /// the `\n` or `0` if none.
    std::size_t line_terminated_frame_size() const noexcept;

    ConnectionClass                              cls_;
    application::ports::IConnectionEgress*       out_ = nullptr;
    std::vector<std::byte>                       rx_;
    std::vector<LegacyFrame>                     dispatched_;
    bool                                         closed_ = false;
};

}  // namespace pvpgn::integration::legacy_bnetd
