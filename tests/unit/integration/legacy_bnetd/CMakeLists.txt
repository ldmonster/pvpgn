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

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_chatevent_compose_bridge
    SOURCES send_chatevent_compose_bridge_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_send_chatevent_compose_e2e
    SOURCES send_chatevent_compose_e2e_test.cpp
    DEPS    integration_legacy_bnetd
            protocol_bnet)
