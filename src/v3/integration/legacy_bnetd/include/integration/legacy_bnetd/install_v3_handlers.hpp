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

/// Installs the legacy-bnetd implementation of the v3
/// `pvpgn_v3_send_packet_try` ABI (wraps bytes in a
/// `packet_class_raw` `t_packet` and pushes via
/// `conn_push_outqueue`). No-op on second call. Must be available
/// before any ported v3 handler attempts to reply through the
/// bridge.
void install_send_packet_handler();

/// Installs the legacy-bnetd implementation of the v3
/// `pvpgn_v3_init_conn_apply` ABI (the byte-1 connection-class
/// state-machine transitions plus the D2CS_BNETD realmlist check
/// and `handle_d2cs_init` call). No-op on second call. Must be
/// available before any client opens a TCP socket if the legacy
/// handler is to delegate to v3.
void install_init_conn_apply_handler();

/// Installs the legacy-bnetd implementation of the v3 ads
/// dispatchers (`pvpgn_v3_ads_pick_apply` /
/// `pvpgn_v3_ads_click_apply`). Backed by the legacy `AdBannerList`
/// global; once registered the legacy `_client_adreq` /
/// `_client_adclick2` handlers may delegate to v3 instead of
/// consulting `AdBannerList` directly. No-op on second call.
void install_ads_handlers();

/// Installs the legacy-bnetd implementation of the v3 realm-list
/// dispatcher (`pvpgn_v3_realm_list_apply`). Backed by the legacy
/// `realmlist()` global; once registered the legacy
/// `_client_realmlistreq` / `_client_realmlistreq110` handlers may
/// delegate to v3 instead of iterating `realmlist()` and building
/// the entry array themselves. No-op on second call.
void install_realm_list_handler();

}  // namespace pvpgn::integration::legacy_bnetd
