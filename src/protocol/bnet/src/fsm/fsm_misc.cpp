// SPDX-License-Identifier: GPL-2.0-or-later
/// @file fsm_misc.cpp
/// BnetFsm — advisory / no-op handlers.
///
/// These messages carry no state-machine semantics: they are accepted
/// silently in any non-Closing state so the wire stays alive.
///
///   on(Null)               — SID_NULL (0x00) keepalive
///   on(Ping)               — SID_PING (0x25) echo
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
///   on(ChangeClient)       — SID_CHANGECLIENT (0x68)
///   on(CdKey3Request)      — SID_CDKEY3 (0x52)

#include "fsm/fsm_internal.hpp"

namespace pvpgn::protocol::bnet {

core::Status<> BnetFsm::on(const Null&) {
    // Keepalive in every state. No reply required.
    return core::ok();
}

core::Status<> BnetFsm::on(const Ping& p) {
    // Mirror the cookie verbatim; this is the canonical ECHOREPLY.
    return ctx_->send(ServerMessage{Ping{p.ticks}});
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
    // SERVER_ICONREPLY carrying the icon file's mtime + name ("icons.bni") so the
    // client can fetch it via BNFTP. v3 does not track the file's mtime at the
    // protocol layer, so the timestamp is a placeholder; the filename matches.
    return ctx_->send(ServerMessage{IconReply{
        /*timestamp*/ 0,
        /*filename*/  "icons.bni"}});
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
core::Status<> BnetFsm::on(const ChangeClient&)    { return core::ok(); }

core::Status<> BnetFsm::on(const CdKey3Request&) {
    // CDKEY3 is part of pre-login auth; accept advisorily.
    return core::ok();
}

}  // namespace pvpgn::protocol::bnet
