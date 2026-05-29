// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file realm_list_bridge.hpp
/// Strangler-fig dispatcher for the BNCS realm-list query
/// (CLIENT_REALMLISTREQ / CLIENT_REALMLISTREQ_110).
///
/// The base half (this file + realm_list_bridge.cpp) exposes the C
/// ABI and atomic handler storage; the linked half
/// (realm_list_bridge_link.cpp) walks the legacy `realmlist()`,
/// runs the application dispatcher, and ships the reply through
/// the existing `pvpgn_v3_send_realmlistreply` /
/// `pvpgn_v3_send_realmlistlegacyreply` bridges.

#ifdef __cplusplus
extern "C" {
#endif

/// Build and send the realm-list reply for `conn_ptr`.
///
/// `legacy_format` selects which on-wire reply to emit:
///   1 -> SERVER_REALMLISTREPLY    (older, 7 unknown-fields per entry)
///   0 -> SERVER_REALMLISTREPLY_110 (newer, 1 unknown-field per entry)
///
/// Returns:
///   1  -> reply was sent. Caller MUST skip its own emit path.
///   0  -> handler installed but reply was not sent (encoder /
///         send-packet failure). Caller MUST NOT retry legacy --
///         the connection is already in a bad state.
///   -1 -> no handler installed. Caller falls back to legacy.
int pvpgn_v3_realm_list_apply(void* conn_ptr, int legacy_format) noexcept;

#ifdef __cplusplus
}
#endif

namespace pvpgn::integration::legacy_bnetd {

/// Handler signature: same args as the extern C entry. Returns 1
/// on success, 0 on send-failure.
using RealmListHandler = int (*)(void* conn_ptr, int legacy_format);

void                set_realm_list_handler(RealmListHandler h) noexcept;
RealmListHandler    get_realm_list_handler() noexcept;

/// Legacy adapter that walks `realmlist()`, runs the application
/// dispatcher and forwards through the existing send_realmlist
/// bridges. Idempotent.
void install_legacy_realm_list_handler();

}  // namespace pvpgn::integration::legacy_bnetd
