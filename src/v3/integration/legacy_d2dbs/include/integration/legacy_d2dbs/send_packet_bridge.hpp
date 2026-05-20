// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_packet_bridge.hpp
/// Strangler-fig sink for the legacy d2dbs server: hand a v3-built
/// byte buffer to the legacy `t_d2dbs_connection` write buffer.
///
/// d2dbs differs from bnetd/d2cs: there is no `t_packet`+outqueue
/// machinery. Each connection carries an inline `WriteBuf[kBufferSize]`
/// drained by psock_send. The linked half memcpys the v3 bytes
/// directly into `WriteBuf` at offset `nCharsInWriteBuffer`.

namespace pvpgn::integration::legacy_d2dbs {

/// Maximum body size the bridge will accept. Conservative cap
/// well below the legacy `kBufferSize = 20480`; the linked half
/// additionally honors the per-connection remaining-space check.
inline constexpr unsigned int kSendPacketMaxSize = 3072u;

using SendPacketHandler =
    int (*)(void* conn_ptr, void const* bytes, unsigned int size) noexcept;

void set_send_packet_handler(SendPacketHandler handler) noexcept;
SendPacketHandler get_send_packet_handler() noexcept;

/// Install the legacy-d2dbs implementation. Defined only in
/// `integration_legacy_d2dbs_linked` (requires d2dbs_legacy).
void install_legacy_send_packet_handler() noexcept;

}  // namespace pvpgn::integration::legacy_d2dbs

extern "C" {

/// Stable C ABI: append @p bytes (length @p size) into the
/// d2dbs connection @p conn_ptr (a `t_d2dbs_connection*`) write
/// buffer. Returns 1 on success, 0 on any failure.
int pvpgn_v3_d2dbs_send_packet_try(void* conn_ptr,
                                    void const* bytes,
                                    unsigned int size) noexcept;

int pvpgn_v3_d2dbs_send_packet_available(void) noexcept;

}  // extern "C"
