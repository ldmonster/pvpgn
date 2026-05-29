// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef PVPGN_V3_INTEGRATION_LEGACY_BNETD_SEND_CHATEVENT_COMPOSE_BRIDGE_HPP
#define PVPGN_V3_INTEGRATION_LEGACY_BNETD_SEND_CHATEVENT_COMPOSE_BRIDGE_HPP

// Strangler-fig bridge built on top of the v3 `application_chat`
// `compose_chat_event` module. Lets the legacy bnetd `message_send`
// path skip `message_bnet_format` entirely by feeding the v3 module
// the pre-resolved chat fields and letting it pick the
// SID_CHATEVENT (0x0F) subtype / flags / latency / username / text.
//
// Return codes match the established strangler-fig convention used by
// every other v3 bridge in this directory:
//   1  : v3 encoded + shipped successfully. Caller MUST skip the
//        legacy `conn_push_outqueue` path.
//   0  : v3 declined (no transport handler installed, compose
//        returned a failure for legitimate reasons such as MF_X /
//        me==NULL / me==dst, encode bounds exceeded, etc.). Caller
//        SHOULD fall through to legacy.
//  -1  : transport handler accepted the bytes but reported a write
//        failure. Caller SHOULD fall through to legacy (legacy push
//        will hit the same broken socket and behave consistently
//        from the client's perspective).
//
// All `char const*` arguments may be NULL; the bridge treats NULL as
// the empty string. `legacy_type` is the integer value of the legacy
// `t_message_type` enum; only values 0..15 (the base `LegacyMessageType`
// set) are recognised. Anything else makes the bridge decline (rc=0).

#ifdef __cplusplus
extern "C" {
#endif

int pvpgn_v3_send_chatevent_compose(
    void*        conn_ptr,
    unsigned int legacy_type,
    int          me_present,                 // 0/1
    unsigned int me_flags,
    unsigned int me_latency,
    unsigned int dstflags,
    int          dstflags_mf_x,              // 0/1
    int          dst_eq_me,                  // 0/1
    unsigned int channel_flags_bncflags,
    char const*  chatcharname,               // nullable
    char const*  chatname,                   // nullable
    char const*  playerinfo,                 // nullable
    char const*  text,                       // nullable
    char const*  servername);                // nullable

#ifdef __cplusplus
}
#endif

#endif
