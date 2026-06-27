// SPDX-License-Identifier: GPL-2.0-or-later
/// @file fsm_misc.cpp
/// BnetFsm — advisory / no-op handlers.
///
/// These messages carry no state-machine semantics: they are accepted
/// silently in any non-Closing state so the wire stays alive.
///
///   on(Null)               — SID_NULL (0x00) keepalive
///   on(Ping)               — SID_PING (0x25) CLIENT_ECHOREPLY (latency, no reply)
///   on(UdpOk)              — SID_UDPPINGRESPONSE (0x14)
///   on(AdRequest)          — SID_DISPLAYAD (0x21)
///   on(AdClick)            — SID_CLICKAD (0x22)
///   on(AdAck)              — SID_ADACK (0x23)
///   on(AdClick2Request)    — SID_CLICKAD2 (0x67)
///   on(RegSnoopReply)      — SID_REGSNOOPRESPONSE (0x18)
///   on(SetEmailReply)      — SID_SETEMAILREPLY (0x59)
///   on(IconRequest)        — SID_ICONREQUEST (0x2D)
///   on(GetPasswordRequest) — SID_GETPASSWORDREQ (0x06)
///   on(ChangeEmailRequest) — SID_CHANGEEMAILREQ (0x5A)
///   on(CrashDump)          — SID_CRASHDUMP (0x2E)
///   on(ExtraWork)          — SID_EXTRAWORK (0x4C)
///   on(ReadMemoryReply)    — SID_READMEMORYREPLY (0x1B)
///   on(Unknown1B)          — SID_UNKNOWN_1B
///   on(Unknown24)          — SID_UNKNOWN_24
///   on(ChangeClient)       — SID_CHANGECLIENT (0x5C)
///   on(CdKey3Request)      — SID_CDKEY3 (0x52)

#include "fsm/fsm_internal.hpp"

namespace pvpgn::protocol::bnet {

core::Status<> BnetFsm::on(const Null&) {
    // Keepalive in every state. No reply required.
    return core::ok();
}

core::Status<> BnetFsm::on(const Ping&) {
    // SID_PING (0x25) inbound is CLIENT_ECHOREPLY: the client echoes the cookie
    // the server sent in SERVER_ECHOREQ so the server can measure round-trip
    // latency. The original (_client_echoreply, handle_bnet.cpp) records the
    // latency and sends NOTHING back — it does not bounce the cookie a second
    // time. Echoing it here produced a spurious 0x25 packet the real client
    // never expects, so this is purely advisory (no reply) in every state.
    return core::ok();
}

core::Status<> BnetFsm::on(const UdpOk&) {
    // UDP echo confirmation may arrive in any state after AUTH_INFO; treat it
    // as advisory and never as a protocol violation.
    return core::ok();
}

core::Status<> BnetFsm::on(const AdRequest&) {
    // Banner fetches are advisory; the client polls them irrespective of
    // login state, so we never reject them.
    return core::ok();
}

core::Status<> BnetFsm::on(const AdClick&) {
    return core::ok();
}

core::Status<> BnetFsm::on(const AdAck&) {
    return core::ok();
}

core::Status<> BnetFsm::on(const AdClick2Request&) {
    return core::ok();
}

core::Status<> BnetFsm::on(const RegSnoopReply&) {
    // Reply to a server-side telemetry poke; advisory in every state.
    return core::ok();
}

core::Status<> BnetFsm::on(const SetEmailReply&) {
    // Returned during login flow; the server may send SETEMAILREQ before
    // the login-ok packet, so accept the reply at any state.
    return core::ok();
}

core::Status<> BnetFsm::on(const IconRequest&) {
    // SID_GETICONDATA (0x2D): the original (_client_iconreq) replies with
    // SERVER_ICONREPLY carrying the icon file's mtime + name so the client can
    // fetch it via BNFTP. The filename is chosen by clienttag: WAR3/W3XP clients
    // get prefs_get_war3_iconfile() (default "icons-WAR3.bni"); all others get
    // prefs_get_iconfile() (default "icons.bni"). v3 does not track the file's
    // mtime at the protocol layer, so the timestamp is a placeholder.
    const bool is_war3 = client_tag_ == domain::tags::kWarcraft3 ||
                         client_tag_ == domain::tags::kWar3Xp;
    return ctx_->send(ServerMessage{IconReply{
        /*timestamp*/ 0,
        /*filename*/  is_war3 ? "icons-WAR3.bni" : "icons.bni"}});
}

core::Status<> BnetFsm::on(const GetPasswordRequest&) {
    // Password recovery happens before login; accept in every state.
    return core::ok();
}

core::Status<> BnetFsm::on(const ChangeEmailRequest&) {
    return core::ok();
}

core::Status<> BnetFsm::on(const CrashDump&) {
    // Crash dumps arrive right after auth success; accept advisorily.
    return core::ok();
}

core::Status<> BnetFsm::on(const ExtraWork&) {
    // EXTRAWORK arrives during auth handshake (response to REQUIREDWORK);
    // accept advisorily so it survives whatever state we are in.
    return core::ok();
}

// Anti-cheat memory replies and echo round-trips are advisory traffic that
// can arrive in any post-AUTH_INFO state without altering the session FSM.
core::Status<> BnetFsm::on(const ReadMemoryReply&) { return core::ok(); }
core::Status<> BnetFsm::on(const Unknown1B&)       { return core::ok(); }
core::Status<> BnetFsm::on(const Unknown24&)       { return core::ok(); }

core::Status<> BnetFsm::on(const ChangeClient& m) {
    // SID_CHANGECLIENT (0x5C): the original (_client_changeclient,
    // handle_bnet.cpp) permits a client switch ONLY when the connection's
    // current tag is WAR3XP ("W3XP") and the requested new tag is WARCRAFT3
    // ("WAR3"). Any other combination is logged as an "invalid attempt to
    // change client" and the connection is destroyed. On the valid path it
    // just swaps the clienttag and sends no reply.
    //
    // decode_change_client reads the requested tag little-endian (RD_U32), so
    // the wire bytes "WAR3" arrive byte-reversed relative to ClientTag's
    // big-endian packed form; byte-swap before comparing.
    const std::uint32_t requested_be = __builtin_bswap32(m.clienttag);
    const bool valid = client_tag_ == domain::tags::kWar3Xp &&
                       requested_be == domain::tags::kWarcraft3.packed_be();
    if (!valid) {
        return reject("bnet fsm: invalid CHANGECLIENT");
    }
    client_tag_ = domain::tags::kWarcraft3;
    return core::ok();
}

core::Status<> BnetFsm::on(const CdKey3Request&) {
    // CDKEY3 is the Diablo II 1.08+ third-key proof, part of pre-login auth.
    // The original (_client_cdkey3) ALWAYS answers with SERVER_CDKEYREPLY3:
    // message = SERVER_CDKEYREPLY3_MESSAGE_OK (0x00) and an EMPTY trailing
    // string (the original sends "" there, not the owner). The client expects
    // this reply, so a silent accept would stall the handshake.
    return ctx_->send(ServerMessage{CdKey3Reply{
        /*message*/    0u,  // SERVER_CDKEYREPLY3_MESSAGE_OK
        /*owner_name*/ ""}});
}

}  // namespace pvpgn::protocol::bnet
