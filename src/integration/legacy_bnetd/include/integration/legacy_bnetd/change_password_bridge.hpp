// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file change_password_bridge.hpp
/// Strangler entry-point scaffold for v3 to take over the legacy
/// `CLIENT_CHANGEPASSREQ` flow (Batch 28b).
///
/// Today this hook is a no-op: `pvpgn_v3_change_password_try`
/// always returns 0 (legacy path runs unchanged) because the
/// `ChangePasswordUseCase` requires a one-shot `BNHash` for the
/// current password, while CLIENT_CHANGEPASSREQ ships a *double-
/// hashed* `oldpassword_hash2` derived from ticks + sessionkey.
/// A future batch will either:
///   * teach the use-case to consume the double-hash transcript
///     directly, or
///   * route a verified pre-image into the use-case from the
///     bridge after the legacy double-hash check passes.
///
/// Until then the hook exists purely so the legacy handler can call
/// it without touching the linker graph again.

namespace pvpgn::integration::legacy_bnetd {

using ChangePasswordHandler =
    int (*)(void* conn_ptr,
            void const* packet_body,
            unsigned int packet_size) noexcept;

void set_change_password_handler(ChangePasswordHandler handler) noexcept;

}  // namespace pvpgn::integration::legacy_bnetd

extern "C" {

/// Stable C ABI. Returns 1 if v3 fully handled the request
/// (including pushing the reply packet); 0 to fall through to the
/// legacy path. Gated by `PVPGN_V3_CHANGEPW=1` at the registration
/// site -- without the env var the slot stays nullptr and the hook
/// returns 0.
int pvpgn_v3_change_password_try(void* conn_ptr,
                                 void const* packet_body,
                                 unsigned int packet_size) noexcept;

}  // extern "C"
