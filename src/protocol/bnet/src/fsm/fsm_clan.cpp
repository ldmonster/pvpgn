// SPDX-License-Identifier: GPL-2.0-or-later
/// @file fsm_clan.cpp
/// BnetFsm — clan and arranged-team handlers.
///
/// All handlers delegate to require_clan_state() (LoggedIn|InChat|InGame)
/// and return ok() as stubs until Phase-5 clan use-cases are wired in.
///
///   on(ClanCreateRequest)             — SID_CLANCREATEREQ (0x70)
///   on(ClanDisbandRequest)            — SID_CLANDISBANDREQ (0x74)
///   on(ClanNewChiefRequest)           — SID_CLANNEWCHIEF (0x7C)
///   on(ClanInviteRequest)             — SID_CLANINVITEREQ (0x77)
///   on(ClanMemberRemoveRequest)       — SID_CLANMEMBERREMOVEREQ (0x78)
///   on(ClanMemberRankUpdateRequest)   — SID_CLANMEMBERRANKUPDATE (0x7D)
///   on(ClanMotdChange)                — SID_CLANMOTDCHG (0x7E)
///   on(ClanMotdRequest)               — SID_CLANMOTDREQ (0x7F)
///   on(ClanCreateInviteRequest)       — SID_CLANCREATEINVITEREQ (0x71)
///   on(ClanCreateInviteResponse)      — SID_CLANCREATEINVITEREPLY (0x72)
///   on(ClanInvite2Response)           — SID_CLANINVITE2REPLY (0x79)
///   on(ClanMemberListRequest)         — SID_CLANMEMBERLISTREQ (0x7A)
///   on(ArrangedTeamFriendScreenRequest)  — SID_AT_FRIENDSCREEN (0x60)
///   on(ArrangedTeamInviteFriendRequest)  — SID_AT_INVITE_FRIEND (0x61)
///   on(ArrangedTeamAcceptDeclineInvite)  — SID_AT_ACCEPT_DECLINE (0x62)
///   on(ArrangedTeamAcceptInvite)         — SID_AT_ACCEPT_INVITE (0x63)

#include "fsm/fsm_internal.hpp"

namespace pvpgn::protocol::bnet {

core::Status<> BnetFsm::on(const ClanCreateRequest& m) {
    if (auto s = require_clan_state(state_, "bnet fsm: CLAN_CREATE before login"); !s)
        return s;
    // The original (clan_get_possible_member) ALWAYS answers SID_CLANCREATEREQ.
    // For a clanless account requesting an unused tag it replies CHECK_OK (0x00)
    // followed by the list of eligible (mutual + online) friends — empty when the
    // account has none. v3 has no clan backend, so an account is always clanless
    // and a tag is never taken: reply CHECK_OK with an empty candidate list. A
    // real WC3 client opening the create-clan dialog hangs without this reply.
    // (Populating the candidate-friend list is a refinement; the common path —
    // no online mutual friends — matches the oracle byte-for-byte.)
    ClanCreateReply reply;
    reply.cookie       = m.cookie;
    reply.check_result = 0x00;  // SERVER_CLAN_CREATEREPLY_CHECK_OK
    return ctx_->send(ServerMessage{reply});
}
core::Status<> BnetFsm::on(const ClanDisbandRequest&) {
    return require_clan_state(state_, "bnet fsm: CLAN_DISBAND before login");
}
core::Status<> BnetFsm::on(const ClanNewChiefRequest&) {
    return require_clan_state(state_, "bnet fsm: CLAN_NEWCHIEF before login");
}
core::Status<> BnetFsm::on(const ClanInviteRequest&) {
    return require_clan_state(state_, "bnet fsm: CLAN_INVITE before login");
}
core::Status<> BnetFsm::on(const ClanMemberRemoveRequest&) {
    return require_clan_state(state_, "bnet fsm: CLAN_MEMBER_REMOVE before login");
}
core::Status<> BnetFsm::on(const ClanMemberRankUpdateRequest&) {
    return require_clan_state(state_, "bnet fsm: CLAN_RANKUPDATE before login");
}
core::Status<> BnetFsm::on(const ClanMotdChange&) {
    return require_clan_state(state_, "bnet fsm: CLAN_MOTDCHG before login");
}
core::Status<> BnetFsm::on(const ClanMotdRequest&) {
    return require_clan_state(state_, "bnet fsm: CLAN_MOTDREQ before login");
}
core::Status<> BnetFsm::on(const ClanCreateInviteRequest&) {
    return require_clan_state(state_, "bnet fsm: CLAN_CREATEINVITE_REQ before login");
}
core::Status<> BnetFsm::on(const ClanCreateInviteResponse&) {
    return require_clan_state(state_, "bnet fsm: CLAN_CREATEINVITE_REPLY before login");
}
core::Status<> BnetFsm::on(const ClanInvite2Response&) {
    return require_clan_state(state_, "bnet fsm: CLAN_INVITE2_REPLY before login");
}
core::Status<> BnetFsm::on(const ClanMemberListRequest&) {
    return require_clan_state(state_, "bnet fsm: CLANMEMBERLIST_REQ before login");
}
core::Status<> BnetFsm::on(const ArrangedTeamFriendScreenRequest&) {
    return require_clan_state(state_, "bnet fsm: AT_FRIENDSCREEN before login");
}
core::Status<> BnetFsm::on(const ArrangedTeamInviteFriendRequest&) {
    return require_clan_state(state_, "bnet fsm: AT_INVITE_FRIEND before login");
}
core::Status<> BnetFsm::on(const ArrangedTeamAcceptDeclineInvite&) {
    return require_clan_state(state_, "bnet fsm: AT_ACCEPT_DECLINE before login");
}
core::Status<> BnetFsm::on(const ArrangedTeamAcceptInvite&) {
    return require_clan_state(state_, "bnet fsm: AT_ACCEPT_INVITE before login");
}

}  // namespace pvpgn::protocol::bnet
