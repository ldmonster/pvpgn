// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file init_conn_bridge.hpp
/// Strangler-fig observer for the legacy `handle_init_packet`
/// byte-1 dispatch.
///
/// Wraps `pvpgn::application::init::dispatch_init_conn` behind a
/// stable C ABI so the legacy bnetd handler can ask v3 what it
/// would decide for a given connection-class byte, compare it
/// against its own switch, and log any disagreement. No state is
/// changed; v3 is purely an observer in this slice.
///
/// The integer values returned in `*out_decision` mirror
/// `pvpgn::application::init::InitDecision`. They are part of the C
/// ABI of this bridge and MUST NOT be renumbered.

#include <cstdint>

namespace pvpgn::integration::legacy_bnetd {

/// Mirror of `application::init::InitDecision` for callers that
/// cannot include C++ headers (legacy bnetd source uses these via
/// `kInitDecisionXxx`).
inline constexpr std::uint8_t kInitDecisionBnet      = 0u;
inline constexpr std::uint8_t kInitDecisionFile      = 1u;
inline constexpr std::uint8_t kInitDecisionBot       = 2u;
inline constexpr std::uint8_t kInitDecisionTelnet    = 3u;
inline constexpr std::uint8_t kInitDecisionD2csBnetd = 4u;
inline constexpr std::uint8_t kInitDecisionRejected  = 0xffu;

/// Apply-handler signature. The handler MUST mutate the connection
/// state for accepted decisions (typically by calling
/// `conn_set_state` / `conn_set_class`) and return one of:
///   *  1 -- the request was fully handled; caller can finish OK.
///   *  0 -- the handler declined (e.g. v3 chose to fall through);
///           caller should continue with the legacy path.
///   * -1 -- the request was handled but failed (e.g. realmlist
///           reject for D2CS_BNETD); caller should close the
///           connection.
using InitConnApplyHandler =
    int (*)(void* conn_ptr, std::uint8_t cclass) noexcept;

/// Register the apply-side handler. Pass nullptr to clear. Thread-
/// safe; last writer wins.
void set_init_conn_apply_handler(InitConnApplyHandler handler) noexcept;

/// Inspect the currently registered handler (mainly for tests).
InitConnApplyHandler get_init_conn_apply_handler() noexcept;

/// Installs the legacy-bnetd implementation of the apply handler
/// (`conn_set_state` + `conn_set_class` plus the D2CS_BNETD
/// realmlist check + `handle_d2cs_init`). Defined only in
/// `integration_legacy_bnetd_linked`.
void install_legacy_init_conn_apply_handler() noexcept;

}  // namespace pvpgn::integration::legacy_bnetd

extern "C" {

/// Ask v3 what dispatch decision it would make for connection-class
/// byte @p cclass. Always writes a value to @p out_decision (one of
/// `kInitDecisionXxx`). May be called with @p out_decision == NULL,
/// in which case the function still returns the accept/reject
/// classification.
///
/// Returns:
///   * 1 if v3 would accept the byte (any non-`kInitDecisionRejected`
///     value);
///   * 0 if v3 would reject the byte.
int pvpgn_v3_init_conn_decide(std::uint8_t cclass,
                              std::uint8_t* out_decision) noexcept;

/// Apply v3's dispatch decision for @p cclass to @p conn_ptr (a
/// `t_connection*`). Calls into the legacy-aware handler registered
/// via `install_legacy_init_conn_apply_handler`.
///
/// Returns the handler's return code: 1 on success, 0 if the
/// handler declined (caller should fall through to legacy), -1 on
/// handled-but-failed (caller should close). Also returns 0 if no
/// handler has been installed, if @p conn_ptr is NULL, or if v3
/// would reject @p cclass outright (the handler is never invoked
/// for rejected bytes).
int pvpgn_v3_init_conn_apply(void* conn_ptr,
                             std::uint8_t cclass) noexcept;

}  // extern "C"
