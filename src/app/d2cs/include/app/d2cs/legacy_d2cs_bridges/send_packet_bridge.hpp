// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_packet_bridge.hpp
/// Sink for the legacy d2cs server: hand a v3-built
/// byte buffer to the legacy `t_d2cs_connection` out-queue.
///
/// Pattern parity with
/// `integration/legacy_bnetd/send_packet_bridge.hpp`. This header is
/// safe to include from any v3 TU; it has no dependency on the
/// legacy d2cs headers. The actual legacy `packet_create` /
/// `conn_d2cs_outqueue` work lives in a `_link.cpp` translation
/// unit compiled only when the legacy d2cs library is in the same
/// build (introduced in a future scaffolding round).
///
/// Returns 1 on success (bytes were enqueued; caller MUST NOT also
/// run legacy send code for the same logical packet). Returns 0 on
/// any failure (legacy fall-back MAY run).

namespace pvpgn::integration::legacy_d2cs {

/// Maximum body size the bridge will accept. Mirrors the
/// `MAX_PACKET_SIZE` used by legacy d2cs (`src/common/field_sizes.h`).
inline constexpr unsigned int kSendPacketMaxSize = 3072u;

using SendPacketHandler =
    int (*)(void* conn_ptr, void const* bytes, unsigned int size) noexcept;

/// Register the legacy-aware sink. Pass nullptr to disable (the C
/// entry-point will then always return 0). Thread-safe; the last
/// writer wins.
void set_send_packet_handler(SendPacketHandler handler) noexcept;

/// Inspect the currently registered handler (mainly for tests).
SendPacketHandler get_send_packet_handler() noexcept;

/// Install the legacy-d2cs implementation. Defined only in the
/// (future) `integration_legacy_d2cs_linked` translation unit;
/// calling it from a build without legacy d2cs is a link-time
/// error by design.

}  // namespace pvpgn::integration::legacy_d2cs

extern "C" {

/// Stable C ABI: enqueue @p bytes (length @p size, in bytes) on the
/// legacy d2cs connection @p conn_ptr (a `t_d2cs_connection*`).
///
/// Returns:
///   * 1 -- fully enqueued; legacy MUST NOT also send this packet.
///   * 0 -- failure (NULL inputs, zero or oversize @p size, or no
///          handler installed). Caller may fall back to its own
///          send logic.
///
/// The buffer @p bytes is copied; ownership is **not** transferred.
int pvpgn_v3_d2cs_send_packet(void* conn_ptr,
                                  void const* bytes,
                                  unsigned int size) noexcept;

/// Cheap probe: 1 if a handler is currently registered, 0 otherwise.
int pvpgn_v3_d2cs_send_packet_available(void) noexcept;

}  // extern "C"
