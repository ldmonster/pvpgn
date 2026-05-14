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
