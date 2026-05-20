// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_messagebox_bridge.hpp
/// Strangler-fig hook for SERVER_MESSAGEBOX (SID 0x19) — a modal
/// dialog push from server to client.
///
/// Wire: 4-byte bnet header (FF 19 size_LE) | u32 style LE | text \0 | caption \0.
///
/// Single legacy call site: src/bnetd/message.cpp messagebox_show().

extern "C" {

/// Build a SERVER_MESSAGEBOX via the v3 codec and dispatch via the
/// registered legacy_bnetd send-packet handler.
///
/// Returns:
///   * 1 -- handled (caller MUST skip legacy emit + outqueue push).
///   * 0 -- not handled (legacy fallback runs).
int pvpgn_v3_send_messagebox(void* conn_ptr,
                              unsigned int style,
                              char const* text,
                              char const* caption) noexcept;

}  // extern "C"
