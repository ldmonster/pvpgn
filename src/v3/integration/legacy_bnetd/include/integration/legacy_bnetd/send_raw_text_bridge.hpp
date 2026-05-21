// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_raw_text_bridge.hpp
/// Strangler-fig hook for raw-text (packet_class_raw) outbound sends used by
/// the Bot and Telnet protocol handlers.
///
/// Both `handle_bot_packet` and `handle_telnet_packet` send plain text strings
/// (login prompts, error messages, success acknowledgements) as raw packets.
/// All 11 `packet_create(packet_class_raw)` sites in each file reduce to one
/// of three payload shapes:
///
///   1. prefix + "\r\nPassword: "   (username echo + password prompt)
///   2. "\r\nLogin failed.\r\n\r\nUsername: "  (tempa — login failure)
///   3. "\r\nAccount has no bot access.\r\n\r\nUsername: "  (tempb)
///   4. "\r\n"                      (login success acknowledgement)
///
/// This bridge encodes an arbitrary NUL-terminated text string as a raw
/// packet and dispatches it through the registered send_packet handler.
///
/// Wire layout: the bytes of `text` (without the NUL terminator) are sent
/// verbatim — exactly what `packet_append_ntstring` produces for a raw packet.
///
/// Returns:
///   1  -> packet forwarded; legacy caller MUST skip its own emit path.
///   0  -> no handler, null input, or handler declined; fall back to legacy.
///   -1 -> handler installed but reported a hard failure.

#include <cstddef>

extern "C" {

/// Build a raw-text packet from `text` (without NUL) and push it to the
/// connection's out-queue via the registered send_packet handler.
///
/// @param conn_ptr  Opaque legacy `t_connection*`. nullptr -> returns 0.
/// @param text      NUL-terminated string to send. nullptr -> returns 0.
///                  Empty string -> sends a zero-byte payload (returns 1 if
///                  handler accepts it).
int pvpgn_v3_send_raw_text(void* conn_ptr,
                            char const* text) noexcept;

/// Variant that concatenates two strings before sending.
/// Equivalent to sending `prefix` immediately followed by `suffix` as a
/// single raw packet (no NUL between them).
///
/// @param conn_ptr  Opaque legacy `t_connection*`. nullptr -> returns 0.
/// @param prefix    First part of the text. nullptr treated as "".
/// @param suffix    Second part of the text. nullptr treated as "".
int pvpgn_v3_send_raw_text2(void* conn_ptr,
                             char const* prefix,
                             char const* suffix) noexcept;

}  // extern "C"
