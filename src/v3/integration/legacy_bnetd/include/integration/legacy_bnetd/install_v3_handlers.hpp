// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file install_v3_handlers.hpp
/// Opt-in composition-root helpers wiring the v3 use-cases into the
/// strangler-fig bridges (Batch 30a/30b). Each install function is
/// gated by its own env var at the call site; calling these from
/// `bnetd/server.cpp` is the *only* way to flip the bridges from
/// scaffold-only to live.
///
/// Installed handlers are leaked intentionally for process lifetime
/// (they reference singleton dependencies that also live forever).

namespace pvpgn::integration::legacy_bnetd {

/// Builds `BnetSessionHasher` + `LegacyAccountRepository` +
/// `ChangePasswordUseCase` and installs a handler into the
/// `change_password_bridge`. Must be called *after* the legacy
/// `accountlist` is initialised. No-op on second call.
void install_change_password_handler();

/// Builds `BnetSessionHasher` + `LegacyAccountRepository` +
/// `LoginUser` (with no-op session registry + event bus) and
/// installs a handler into the `login_user_bridge`. No-op on
/// second call.
void install_login_user_handler();

}  // namespace pvpgn::integration::legacy_bnetd
