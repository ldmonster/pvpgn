pvpgn_v3_add_test(test_integration_legacy_bnetd_handler
    SOURCES legacy_protocol_handler_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_frame_router
    SOURCES legacy_bnet_frame_router_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_strangler
    SOURCES bnet_strangler_handler_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_anongame_bootstrap
    SOURCES anongame_bootstrap_test.cpp
    DEPS    integration_legacy_bnetd infra_compression)

pvpgn_v3_add_test(test_integration_legacy_bnetd_strangler_parity
    SOURCES strangler_parity_test.cpp
    DEPS    protocol_bnet)

pvpgn_v3_add_test(test_integration_legacy_bnetd_chat_reply_sink_strings
    SOURCES legacy_chat_reply_sink_strings_test.cpp
    DEPS    application_chat application_i18n integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_chat_reply_sink_fixture
    SOURCES legacy_chat_reply_sink_fixture_test.cpp
    DEPS    application_chat application_i18n integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_account_repository_hex
    SOURCES legacy_account_repository_hex_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_packet_bridge
    SOURCES send_packet_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_init_conn_bridge
    SOURCES init_conn_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_authreq1_server_bridge
    SOURCES send_authreq1_server_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_authreply1_bridge
    SOURCES send_authreply1_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_authreply109_bridge
    SOURCES send_authreply109_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_authinfo_reply_bridge
    SOURCES send_authinfo_reply_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_loginreply_bridge
    SOURCES send_loginreply_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_loginreply_w3_bridge
    SOURCES send_loginreply_w3_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_logonproof_reply_bridge
    SOURCES send_logonproof_reply_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_createacct_reply_bridge
    SOURCES send_createacct_reply_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_iconreply_bridge
    SOURCES send_iconreply_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_channellist_bridge
    SOURCES send_channellist_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_channel_state_bridge
    SOURCES channel_state_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_game_report_bridge
    SOURCES game_report_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_startgame_bridge
    SOURCES startgame_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_gamelist_join_bridge
    SOURCES gamelist_join_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_anongame_dispatch_bridge
    SOURCES anongame_dispatch_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_anongame_entry_bridge
    SOURCES anongame_entry_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_anongame_infos_bridge
    SOURCES anongame_infos_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_anongame_infos_get_bridge
    SOURCES anongame_infos_get_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_clan_dispatch_bridge
    SOURCES clan_dispatch_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_clan_send_bridge
    SOURCES clan_send_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_friends_dispatch_bridge
    SOURCES friends_dispatch_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_auth_dispatch_bridge
    SOURCES auth_dispatch_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_keepalive_dispatch_bridge
    SOURCES keepalive_dispatch_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_realm_dispatch_bridge
    SOURCES realm_dispatch_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_account_dispatch_bridge
    SOURCES account_dispatch_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_profile_dispatch_bridge
    SOURCES profile_dispatch_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_ladder_dispatch_bridge
    SOURCES ladder_dispatch_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_d2_character_dispatch_bridge
    SOURCES d2_character_dispatch_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_telemetry_dispatch_bridge
    SOURCES telemetry_dispatch_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_ad_dispatch_bridge
    SOURCES ad_dispatch_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_progident_dispatch_bridge
    SOURCES progident_dispatch_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_gameport_dispatch_bridge
    SOURCES gameport_dispatch_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_cdkey_dispatch_bridge
    SOURCES cdkey_dispatch_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_passemail_dispatch_bridge
    SOURCES passemail_dispatch_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_handshake_dispatch_bridge
    SOURCES handshake_dispatch_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_stub_dispatch_bridge
    SOURCES stub_dispatch_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_file_dispatch_bridge
    SOURCES file_dispatch_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_handshake_bridge
    SOURCES send_handshake_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_echoreq_bridge
    SOURCES send_echoreq_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_messagebox_bridge
    SOURCES send_messagebox_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_friend_acks_bridges
    SOURCES send_friend_acks_bridges_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_antihack_bridges
    SOURCES send_antihack_bridges_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_clan_invitereply_bridge
    SOURCES send_clan_invitereply_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_clan_membernewchief_reply_bridge
    SOURCES send_clan_membernewchief_reply_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_clanmember_remove_reply_bridge
    SOURCES send_clanmember_remove_reply_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_clanmember_rankupdate_reply_bridge
    SOURCES send_clanmember_rankupdate_reply_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_changepassack_bridge
    SOURCES send_changepassack_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_chatevent_compose_bridge
    SOURCES send_chatevent_compose_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_chatevent_compose_e2e
    SOURCES send_chatevent_compose_e2e_test.cpp
    DEPS    integration_legacy_bnetd
            protocol_bnet)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_cdkey_reply_bridge
    SOURCES send_cdkey_reply_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_fileinforeply_bridge
    SOURCES send_fileinforeply_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_passchange_bridge
    SOURCES send_passchange_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_statsreply_bridge
    SOURCES send_statsreply_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_friendslist_bridge
    SOURCES send_friendslist_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_friendinfo_bridge
    SOURCES send_friendinfo_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_atfriendscreen_bridge
    SOURCES send_atfriendscreen_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_atinvitefriend_bridge
    SOURCES send_atinvitefriend_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_atacceptdecline_bridge
    SOURCES send_atacceptdecline_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_motdw3_bridge
    SOURCES send_motdw3_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_realmlist_bridge
    SOURCES send_realmlist_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_realmlistlegacy_bridge
    SOURCES send_realmlistlegacy_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_claninfo_bridge
    SOURCES send_claninfo_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_profilereply_bridge
    SOURCES send_profilereply_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_realmjoin_bridge
    SOURCES send_realmjoin_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_charlistreply_bridge
    SOURCES send_charlistreply_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_adreply_bridge
    SOURCES send_adreply_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_adclick2reply_bridge
    SOURCES send_adclick2reply_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_playerinforeply_bridge
    SOURCES send_playerinforeply_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_gamelistreply_bridge
    SOURCES send_gamelistreply_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_startgame1ack_bridge
    SOURCES send_startgame1ack_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_startgame3ack_bridge
    SOURCES send_startgame3ack_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_startgame4ack_bridge
    SOURCES send_startgame4ack_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_ladderreply_bridge
    SOURCES send_ladderreply_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_laddersearchreply_bridge
    SOURCES send_laddersearchreply_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_mapauthreply1_bridge
    SOURCES send_mapauthreply1_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_mapauthreply2_bridge
    SOURCES send_mapauthreply2_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_clandisbandreply_bridge
    SOURCES send_clandisbandreply_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_clancreateinviteforward_bridge
    SOURCES send_clancreateinviteforward_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_clancreateinvitereply_bridge
    SOURCES send_clancreateinvitereply_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_d2cs_link_dispatch_bridge
    SOURCES d2cs_link_dispatch_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_bot_dispatch_bridge
    SOURCES bot_dispatch_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_telnet_dispatch_bridge
    SOURCES telnet_dispatch_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_wol_dispatch_bridge
    SOURCES wol_dispatch_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_irc_dispatch_bridge
    SOURCES irc_dispatch_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_connection_dispatch_bridge
    SOURCES connection_dispatch_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_message_dispatch_bridge
    SOURCES message_dispatch_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_server_dispatch_bridge
    SOURCES server_dispatch_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_command_dispatch_bridge
    SOURCES command_dispatch_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_clan_motdreply_bridge
    SOURCES send_clan_motdreply_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_encode_clanmemberupdate_bridge
    SOURCES encode_clanmemberupdate_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_clan_bridges
    SOURCES clan_bridges_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_raw_text_bridge
    SOURCES send_raw_text_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_anongame_search_reply_bridge
    SOURCES send_anongame_search_reply_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_anongame_found_bridge
    SOURCES send_anongame_found_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_w3route_bridge
    SOURCES send_w3route_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_d2cs_bnetd_bridges
    SOURCES send_d2cs_bnetd_bridges_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_anongame_cancel_bridge
    SOURCES send_anongame_cancel_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_udptest_bridge
    SOURCES send_udptest_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_file_bridge
    SOURCES send_file_bridge_test.cpp
    DEPS    integration_legacy_bnetd)
