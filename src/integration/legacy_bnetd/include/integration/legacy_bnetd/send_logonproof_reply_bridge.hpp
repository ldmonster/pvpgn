// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef PVPGN_V3_INTEGRATION_LEGACY_BNETD_SEND_LOGONPROOF_REPLY_BRIDGE_HPP
#define PVPGN_V3_INTEGRATION_LEGACY_BNETD_SEND_LOGONPROOF_REPLY_BRIDGE_HPP

#ifdef __cplusplus
extern "C" {
#endif

// Strangler-fig bridge for SERVER_LOGONPROOFREPLY (0x54).
//
// Wire layout:
//   header(4) + u32 response + 20-byte server_password_proof
//   + optional NUL-terminated custom_reason string (only legacy CUSTOM
//     branch -- account-locked).
//
// `server_password_proof` may be nullptr -> 20 zero bytes.
// `custom_reason` may be nullptr or "" -> no trailing string appended
// (caller is responsible for matching legacy's append-string gate).
//
// Returns 1 = shipped via v3 transport (caller must skip legacy push),
//         0 = decline / no handler / build failure,
//        -1 = hard transport failure.
int pvpgn_v3_send_logonproof_reply(
    void* conn_ptr,
    unsigned int response,
    unsigned char const* server_password_proof,
    char const* custom_reason);

#ifdef __cplusplus
}
#endif

#endif  // PVPGN_V3_INTEGRATION_LEGACY_BNETD_SEND_LOGONPROOF_REPLY_BRIDGE_HPP
