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

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_chatevent_compose_bridge
    SOURCES send_chatevent_compose_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_chatevent_compose_e2e
    SOURCES send_chatevent_compose_e2e_test.cpp
    DEPS    integration_legacy_bnetd
            protocol_bnet)
