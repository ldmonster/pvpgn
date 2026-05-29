// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_packet_bridge.hpp
/// Strangler-fig sink: hand a fully-formed v3 byte buffer to the
/// legacy `t_connection` out-queue.
///
/// Use case: a v3 handler builds an outbound packet with
/// `protocol::Writer` (or any equivalent), calls `.view()` to obtain
/// the raw bytes, and then needs the bytes delivered through the
/// existing legacy IO loop (fdwatch / `psock_send` / throttling /
/// disconnect handling).
///
/// This bridge wraps the bytes in a `packet_class_raw` `t_packet`
/// and pushes it onto the connection's out-queue. The legacy IO
/// loop drains the queue exactly as it does for legacy-built
/// packets. No new IO path is introduced; the legacy code remains
/// the single send authority during the transition.
///
/// Available only in `integration_legacy_bnetd_linked` (requires
/// the legacy `bnetd_legacy` library to be present).
///
/// Returns 1 on success (bytes were enqueued; caller MUST NOT also
/// run legacy send code for the same logical packet). Returns 0 on
/// any failure (legacy fall-back MAY run), with the failure logged.

namespace pvpgn::integration::legacy_bnetd {

/// Maximum body size the bridge will accept, mirroring legacy
/// `MAX_PACKET_SIZE` in `src/common/field_sizes.h`. Larger payloads
/// are rejected.
inline constexpr unsigned int kSendPacketMaxSize = 3072u;

using SendPacketHandler =
    int (*)(void* conn_ptr, void const* bytes, unsigned int size) noexcept;

/// Register the legacy-aware sink. Pass nullptr to disable (the C
/// entry-point will then always return 0). Thread-safe; the last
/// writer wins.
void set_send_packet_handler(SendPacketHandler handler) noexcept;

/// Inspect the currently registered handler (mainly for tests).
SendPacketHandler get_send_packet_handler() noexcept;

/// Install the legacy-bnetd implementation that wraps bytes in a
/// `packet_class_raw` `t_packet` and pushes via
/// `conn_push_outqueue`. Defined only in
/// `integration_legacy_bnetd_linked`; calling this from a build
/// without legacy bnetd is a link-time error by design.
void install_legacy_send_packet_handler() noexcept;

}  // namespace pvpgn::integration::legacy_bnetd

extern "C" {

/// Stable C ABI: enqueue @p bytes (length @p size, in bytes) on the
/// legacy connection @p conn_ptr (a `t_connection*`).
///
/// Returns:
///   * 1 — fully enqueued; legacy MUST NOT also send this packet.
///   * 0 — failure (NULL inputs, zero or oversize @p size, or
///         out-of-memory from `packet_create`). Caller may fall
///         back to its own send logic.
///
/// The buffer @p bytes is copied; ownership is **not** transferred.
int pvpgn_v3_send_packet_try(void* conn_ptr,
                             void const* bytes,
                             unsigned int size) noexcept;

/// Cheap probe: returns 1 if a send-packet handler is currently
/// registered (i.e. a subsequent `pvpgn_v3_send_packet_try` call may
/// succeed), 0 otherwise. Intended for all-or-nothing broadcast
/// callers that need to decide whether to dispatch via the v3 sink
/// or fall through to legacy iteration before doing any work. The
/// probe never touches the connection or buffer state.
int pvpgn_v3_send_packet_available(void) noexcept;

}  // extern "C"
