pvpgn_v3_add_test(test_integration_legacy_d2dbs_send_packet_bridge
    SOURCES send_packet_bridge_test.cpp
    DEPS    integration_legacy_d2dbs)

pvpgn_v3_add_test(test_integration_legacy_d2dbs_send_echorequest_bridge
    SOURCES send_echorequest_bridge_test.cpp
    DEPS    integration_legacy_d2dbs)
