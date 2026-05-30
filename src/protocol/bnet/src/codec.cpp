// SPDX-License-Identifier: GPL-2.0-or-later
// codec.cpp — dispatch layer for bnet protocol codec
// Decode/encode implementations live in codec/ subdirectory modules.
// See plans/15-large-file-decomposition-detail.md §8 for rationale.
#include "codec/codec_internal.h"

namespace pvpgn::protocol::bnet {
namespace detail {

// ── Helper implementations ────────────────────────────────────────────────────

core::Failure<core::Error> unimplemented(std::uint8_t code) {
    std::string msg = "bnet codec: unimplemented SID 0x";
    static constexpr char kDigits[] = "0123456789ABCDEF";
    msg.push_back(kDigits[(code >> 4) & 0x0F]);
    msg.push_back(kDigits[code & 0x0F]);
    return core::fail(
        core::Error{core::StatusCode::Unimplemented, std::move(msg)});
}

core::Status<> check_empty_body(const Packet& pkt) {
    if (!pkt.payload.empty()) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "bnet codec: SID_NULL must have empty body"});
    }
    return core::ok();
}

} // namespace detail

// ── decode_client() dispatch ──────────────────────────────────────────────────

core::Result<ClientMessage> decode_client(const Packet& pkt) {
    using namespace detail;
    switch (pkt.header.code) {
        case kSidNull: {
            auto s = check_empty_body(pkt);
            if (!s) return core::fail(s.error());
            return ClientMessage{Null{}};
        }
        case kSidPing: {
            auto m = decode_ping(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidAuthInfo: {
            auto m = decode_auth_info(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidLogonResponse2: {
            auto m = decode_logon_response2(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidJoinChannel: {
            auto m = decode_join_channel(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidEnterChat: {
            auto m = decode_enter_chat_req(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidChatCommand: {
            auto m = decode_chat_command(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidAuthCheck: {
            auto m = decode_auth_check_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidGetAdvListEx: {
            auto m = decode_game_list_req(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidLadderSearch: {
            auto m = decode_ladder_search_req(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidGetFileTime: {
            auto m = decode_file_info_req(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidCdKey2: {
            auto m = decode_cdkey2_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidFriendsList: {
            auto m = decode_friendslist_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidFriendInfo: {
            auto m = decode_friendinfo_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidClanInfo: {
            auto m = decode_claninfo_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidReadUserData: {
            auto m = decode_userdata_read_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidWriteUserData: {
            auto m = decode_userdata_write_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidClanCreate: {
            auto m = decode_clan_create_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidClanDisband: {
            auto m = decode_clan_disband_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidClanMemberNewChief: {
            auto m = decode_clan_newchief_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidClanInvite: {
            auto m = decode_clan_invite_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidClanMemberRemove: {
            auto m = decode_clan_member_remove_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidClanMemberRankUpdate: {
            auto m = decode_clan_member_rank_update_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidClanMotdChange: {
            auto m = decode_clan_motd_change(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidClanMotd: {
            auto m = decode_clan_motd_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidClanCreateInvite: {
            auto m = decode_clan_create_invite_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidClanCreateInvite2: {
            auto m = decode_clan_create_invite_response(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidClanInvite2: {
            auto m = decode_clan_invite2_response(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidClanMemberList: {
            auto m = decode_clan_memberlist_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidArrangedTeamFriendScreen: {
            auto m = decode_arrangedteam_friendscreen_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidArrangedTeamInviteFriend: {
            auto m = decode_arrangedteam_invite_friend_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidArrangedTeamSendInvite: {
            auto m = decode_arrangedteam_accept_decline_invite(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidArrangedTeamAcceptInvite: {
            auto m = decode_arrangedteam_accept_invite(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidStartGame4: {
            auto m = decode_startgame4_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidUdpOk: {
            auto m = decode_udp_ok(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidLadderList: {
            auto m = decode_ladder_list_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidCheckAd: {
            auto m = decode_ad_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidAdClick: {
            auto m = decode_ad_click(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidAdAck: {
            auto m = decode_ad_ack(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidAdClick2: {
            auto m = decode_ad_click2_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidMotd: {
            auto m = decode_motd_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidChannelList: {
            auto m = decode_channel_list_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidLeaveChat: {
            auto m = decode_leave_channel(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidRegSnoop: {
            auto m = decode_regsnoop_reply(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidProfile: {
            auto m = decode_profile_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidSetEmail: {
            auto m = decode_setemail_reply(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidIconReq: {
            auto m = decode_icon_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidGetPassword: {
            auto m = decode_get_password_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidChangeEmail: {
            auto m = decode_change_email_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidCrashDump: {
            auto m = decode_crash_dump(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidCharList: {
            auto m = decode_char_list_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidRealmList: {
            auto m = decode_realm_list_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidRealmJoin: {
            auto m = decode_realm_join_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidWarcraftGeneral: {
            auto m = decode_warcraft_general_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidExtraWork: {
            auto m = decode_extra_work(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidRealmListLegacy: {
            auto m = decode_realm_list_legacy_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidCdKey3: {
            auto m = decode_cdkey3_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidCreateAccount2: {
            auto m = decode_createaccount2_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidLoginW3: {
            auto m = decode_loginw3_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidLogonProofW3: {
            auto m = decode_logonproof_w3_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidPassChange: {
            auto m = decode_passchange_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidPassChangeProof: {
            auto m = decode_passchange_proof_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidCompInfo1: {
            auto m = decode_compinfo1_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidProgIdent: {
            auto m = decode_progident(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidAuthReq1: {
            auto m = decode_authreq1(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidCountryInfo1: {
            auto m = decode_countryinfo1(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidCompInfo2: {
            auto m = decode_compinfo2(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidLogonResponse: {
            auto m = decode_loginreq1(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidCreateAccount1: {
            auto m = decode_createaccount1_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidUnknown2B: {
            auto m = decode_unknown_2b(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidCdKeyLegacy: {
            auto m = decode_cdkey_legacy_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidChangePassword: {
            auto m = decode_changepassword_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidUnknown39: {
            auto m = decode_unknown_39(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidCreateAccount: {
            auto m = decode_createaccount_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidNetGamePort: {
            auto m = decode_netgameport(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidCloseGame: {
            auto m = decode_close_game(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidCloseGame2: {
            auto m = decode_close_game2(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidStartGame1: {
            auto m = decode_startgame1_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidStartGame3: {
            auto m = decode_startgame3_request(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidJoinGame: {
            auto m = decode_join_game(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidGameReport: {
            auto m = decode_game_report(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidReadMemory: {
            auto m = decode_read_memory_reply(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidUnknown1B: {
            auto m = decode_unknown_1b(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidUnknown24: {
            auto m = decode_unknown_24(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidMapAuth1: {
            auto m = decode_mapauthreq1(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidMapAuth2: {
            auto m = decode_mapauthreq2(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidChangeClient: {
            auto m = decode_change_client(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        default:
            return unimplemented(pkt.header.code);
    }
}

// ── decode_server() dispatch ──────────────────────────────────────────────────

core::Result<ServerMessage> decode_server(const Packet& pkt) {
    using namespace detail;
    switch (pkt.header.code) {
        case kSidNull: {
            auto s = check_empty_body(pkt);
            if (!s) return core::fail(s.error());
            return ServerMessage{Null{}};
        }
        case kSidPing: {
            auto m = decode_ping(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidAuthCheck: {
            auto m = decode_auth_check_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidAuthInfo: {
            auto m = decode_auth_info_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidLogonResponse2: {
            auto m = decode_logon_response2_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidEnterChat: {
            auto m = decode_enter_chat_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidChatEvent: {
            auto m = decode_chat_event(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidGetAdvListEx: {
            auto m = decode_game_list_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidLadderSearch: {
            auto m = decode_ladder_search_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidGetFileTime: {
            auto m = decode_file_info_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidCdKey2: {
            auto m = decode_cdkey2_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidFriendsList: {
            auto m = decode_friendslist_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidFriendInfo: {
            auto m = decode_friendinfo_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidFriendAdd: {
            auto m = decode_friendadd_ack(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidFriendDel: {
            auto m = decode_frienddel_ack(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidFriendMove: {
            auto m = decode_friendmove_ack(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidClanInfo: {
            auto m = decode_claninfo_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidReadUserData: {
            auto m = decode_userdata_read_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidClanCreate: {
            auto m = decode_clan_create_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidClanDisband:
        case kSidClanMemberNewChief:
        case kSidClanInvite:
        case kSidClanMemberRemove:
        case kSidClanMemberRankUpdate: {
            auto m = decode_clan_generic_result_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidClanMotd: {
            auto m = decode_clan_motd_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidClanCreateInvite: {
            auto m = decode_clan_create_invite_summary(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidClanCreateInvite2: {
            auto m = decode_clan_create_invite_forward(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidClanInvite2: {
            auto m = decode_clan_invite2_forward(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidClanMemberList: {
            auto m = decode_clan_memberlist_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidClanMemberRemoved: {
            auto m = decode_clan_member_removed_notify(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidClanMemberUpdate: {
            auto m = decode_clan_member_update(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidArrangedTeamFriendScreen: {
            auto m = decode_arrangedteam_friendscreen_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidArrangedTeamInviteFriend: {
            auto m = decode_arrangedteam_invite_friend_ack(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidArrangedTeamMemberDecline: {
            auto m = decode_arrangedteam_member_decline(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidArrangedTeamSendInvite: {
            auto m = decode_arrangedteam_send_invite(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidStartGame4: {
            auto m = decode_startgame4_ack(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidLadderList: {
            auto m = decode_ladder_list_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidCheckAd: {
            auto m = decode_ad_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidAdClick2: {
            auto m = decode_ad_click2_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidMotd: {
            auto m = decode_motd_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidChannelList: {
            auto m = decode_channel_list_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidRegSnoop: {
            auto m = decode_regsnoop_request(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidProfile: {
            auto m = decode_profile_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidSetEmail: {
            auto m = decode_setemail_request(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidIconReq: {
            auto m = decode_icon_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidCharList: {
            auto m = decode_char_list_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidServerList: {
            auto m = decode_server_list(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidMessageBox: {
            auto m = decode_message_box(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidRealmList: {
            auto m = decode_realm_list_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidRealmJoin: {
            auto m = decode_realm_join_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidWarcraftGeneral: {
            auto m = decode_warcraft_general_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidRequiredWork: {
            auto m = decode_required_work(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidRealmListLegacy: {
            auto m = decode_realm_list_legacy_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidCdKey3: {
            auto m = decode_cdkey3_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidCreateAccount2: {
            auto m = decode_createaccount2_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidLoginW3: {
            auto m = decode_loginw3_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidLogonProofW3: {
            auto m = decode_logonproof_w3_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidPassChange: {
            auto m = decode_passchange_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidPassChangeProof: {
            auto m = decode_passchange_proof_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidCompInfo1: {
            auto m = decode_compreply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidProgIdent: {
            auto m = decode_authreq1_server(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidAuthReq1: {
            auto m = decode_authreply1(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidSessionKey1: {
            auto m = decode_sessionkey1(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidSessionKey2: {
            auto m = decode_sessionkey2(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidLogonResponse: {
            auto m = decode_loginreply1(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidCreateAccount1: {
            auto m = decode_createaccount1_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidCdKeyLegacy: {
            auto m = decode_cdkey_legacy_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidChangePassword: {
            auto m = decode_changepassword_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidCreateAccount: {
            auto m = decode_createaccount_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidStartGame1: {
            auto m = decode_startgame1_ack(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidStartGame3: {
            auto m = decode_startgame3_ack(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidReadMemory: {
            auto m = decode_read_memory_request(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidMapAuth1: {
            auto m = decode_mapauthreply1(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidMapAuth2: {
            auto m = decode_mapauthreply2(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        default:
            return unimplemented(pkt.header.code);
    }
}

} // namespace pvpgn::protocol::bnet
