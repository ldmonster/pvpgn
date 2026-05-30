// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef PVPGN_INTEGRATION_LEGACY_BNETD_HANDSHAKE_DISPATCH_BRIDGE_HPP
#define PVPGN_INTEGRATION_LEGACY_BNETD_HANDSHAKE_DISPATCH_BRIDGE_HPP

extern "C" int pvpgn_v3_handshake_dispatch(void* conn_ptr,
                                               char const* op) noexcept;

#endif
