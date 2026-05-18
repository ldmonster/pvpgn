// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef PVPGN_V3_INTEGRATION_LEGACY_BNETD_SEND_CREATEACCT_REPLY_BRIDGE_HPP
#define PVPGN_V3_INTEGRATION_LEGACY_BNETD_SEND_CREATEACCT_REPLY_BRIDGE_HPP

#ifdef __cplusplus
extern "C" {
#endif

// Strangler-fig bridges for SERVER_CREATEACCTREPLY1 (0x2a),
// SERVER_CREATEACCTREPLY2 (0x3d) and SERVER_CREATEACCOUNT_W3 (0x52).
// Identical wire layout:
//   header(4) + u32 result
// Returns 1 = shipped via v3 transport, 0 = decline,
//        -1 = hard transport failure.
int pvpgn_v3_send_createacctreply1(void* conn_ptr, unsigned int result);
int pvpgn_v3_send_createacctreply2(void* conn_ptr, unsigned int result);
int pvpgn_v3_send_createaccount_w3(void* conn_ptr, unsigned int result);

#ifdef __cplusplus
}
#endif

#endif
