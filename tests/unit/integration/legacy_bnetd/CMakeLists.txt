pvpgn_v3_add_test(test_integration_legacy_bnetd_handler
    SOURCES legacy_protocol_handler_test.cpp
    DEPS    integration_legacy_bnetd)

pvpgn_v3_add_test(test_integration_legacy_bnetd_strangler
    SOURCES bnet_strangler_handler_test.cpp
    DEPS    integration_legacy_bnetd)
