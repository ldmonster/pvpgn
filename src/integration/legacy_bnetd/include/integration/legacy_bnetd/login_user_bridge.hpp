// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file login_user_bridge.hpp
/// Strangler entry-point scaffold for v3 to take over the legacy
/// `CLIENT_AUTHCHECK` / login pipeline. Today it is a no-op: the
/// extern-C hook always returns 0 ("not handled, fall through to
/// the legacy path"), and no legacy code calls it yet.
///
/// Batch 27a installs the hook so:
///   * a follow-up batch can wire `handle_bnet.cpp`'s
///     `_client_authcheckreq` -> `pvpgn_v3_login_user_try` without
///     touching the linker graph again;
///   * an `IAccountRepository` adapter (Batch 28) can be plugged
///     in via a separate registration call, mirroring the
///     `set_dispatch` pattern used by the chat-reply sink.

#include "integration/legacy_bnetd/dispatch.hpp"

extern "C" {

/// Stable C ABI. Returns 1 if v3 fully handled the login attempt
/// (including writing any reply packet to the connection);
/// returns 0 to instruct the legacy path to run unchanged.
///
/// `conn_ptr`, `req_body`, `req_size` are passed straight through
/// from the legacy handler. The bridge does not capture or retain
/// any of these pointers beyond the call.
int pvpgn_v3_login_user_try(void* conn_ptr,
                            void const* req_body,
                            unsigned int req_size) noexcept;

}  // extern "C"

namespace pvpgn::integration::legacy_bnetd {

/// Future-facing registration seam. Lit up by Batch 28+ once a real
/// `IAccountRepository` legacy-adapter exists. Calling this today
/// has no effect on behaviour; the scaffold is exposed only so the
/// composition root can hold a stable symbol.
using LoginUserHandler =
    int (*)(void* conn_ptr,
            void const* req_body,
            unsigned int req_size) noexcept;

void set_login_user_handler(LoginUserHandler handler) noexcept;

}  // namespace pvpgn::integration::legacy_bnetd
