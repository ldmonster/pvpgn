pvpgn_v3_add_test(test_integration_legacy_d2cs_send_packet_bridge
    SOURCES send_packet_bridge_test.cpp
    DEPS    integration_legacy_d2cs)

pvpgn_v3_add_test(test_integration_legacy_d2cs_send_loginreply_bridge
    SOURCES send_loginreply_bridge_test.cpp
    DEPS    integration_legacy_d2cs)

pvpgn_v3_add_test(test_integration_legacy_d2cs_send_createcharreply_bridge
    SOURCES send_createcharreply_bridge_test.cpp
    DEPS    integration_legacy_d2cs)

pvpgn_v3_add_test(test_integration_legacy_d2cs_send_charloginreply_bridge
    SOURCES send_charloginreply_bridge_test.cpp
    DEPS    integration_legacy_d2cs)

pvpgn_v3_add_test(test_integration_legacy_d2cs_send_gamereply_bridges
    SOURCES send_gamereply_bridges_test.cpp
    DEPS    integration_legacy_d2cs)

pvpgn_v3_add_test(test_integration_legacy_d2cs_send_handle_bnetd_bridges
    SOURCES send_handle_bnetd_bridges_test.cpp
    DEPS    integration_legacy_d2cs)

pvpgn_v3_add_test(test_integration_legacy_d2cs_send_handle_d2gs_bridges
    SOURCES send_handle_d2gs_bridges_test.cpp
    DEPS    integration_legacy_d2cs)

pvpgn_v3_add_test(test_integration_legacy_d2cs_send_simple_replies
    SOURCES send_simple_replies_test.cpp
    DEPS    integration_legacy_d2cs)

pvpgn_v3_add_test(test_integration_legacy_d2cs_send_listreply_bridges
    SOURCES send_listreply_bridges_test.cpp
    DEPS    integration_legacy_d2cs)

pvpgn_v3_add_test(test_integration_legacy_d2cs_send_ladderreply_bridge
    SOURCES send_d2cs_listreply_encoders_bridges_test.cpp
    DEPS    integration_legacy_d2cs)
